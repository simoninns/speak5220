/*
 * File:        optimiser.cpp
 * Module:      optimiser
 * Purpose:     Searches a phrase's encoder options for the best quality
 *              within the project budget and an optional byte limit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/optimiser/optimiser.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <condition_variable>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_statistics.h"
#include "core/quality/quality_metric.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"

namespace speak5220::core::optimiser {
namespace {

using encoder::EncoderOptions;

// The rate the encoder takes (plan §3.1).
constexpr int kSampleRate = 8000;
constexpr double kBitsPerKilobit = 1000.0;

// The options the search moves along (plan §3.3).
enum class Axis {
  kTargetRms,
  kRepeatLambda,
  kGateDb,
  kPreEmphasis,
  kWindow,
  kLagWindowHz,
  kVoicingThreshold,
  kVoicingEvidence,
  kLevelAgc,
};

// How the descent steps along an axis. The kind of pre-emphasis has no
// step: each round tries the other kind.
struct AxisSteps {
  Axis axis;
  // The first step, and the smallest; values are rounded to `decimals`
  // places, so the options' text stays short.
  double step;
  double min_step;
  int decimals;
};

// The descent's axes, in the order their moves are tried (and so their
// indices, which break ties). Steps of about a tenth of each option's
// useful range, refined to a few percent.
constexpr std::array<AxisSteps, 9> kDescentAxes = {{
    {.axis = Axis::kTargetRms, .step = 2.0, .min_step = 0.5, .decimals = 2},
    {.axis = Axis::kRepeatLambda, .step = 20.0, .min_step = 5.0, .decimals = 1},
    {.axis = Axis::kGateDb, .step = 5.0, .min_step = 1.25, .decimals = 2},
    {.axis = Axis::kPreEmphasis, .step = 0.04, .min_step = 0.01, .decimals = 3},
    {.axis = Axis::kWindow, .step = 40.0, .min_step = 10.0, .decimals = 0},
    {.axis = Axis::kLagWindowHz, .step = 20.0, .min_step = 5.0, .decimals = 1},
    {.axis = Axis::kVoicingThreshold,
     .step = 0.05,
     .min_step = 0.0125,
     .decimals = 4},
    {.axis = Axis::kVoicingEvidence,
     .step = 1.0,
     .min_step = 0.25,
     .decimals = 2},
    {.axis = Axis::kLevelAgc, .step = 0.5, .min_step = 0.125, .decimals = 3},
}};

// The grid (plan §3.3, step 1), around the start.
constexpr double kGridVoicingThreshold = 0.05;
constexpr double kGridVoicingEvidence = 2.0;
constexpr double kGridRepeatLambdaFactor = 2.0;
constexpr double kGridTargetRms = 4.0;
// The first-order coefficient the grid and the descent try in place of the
// chirp pre-emphasis: the "lean" option set's (TESTING.md's reference
// data).
constexpr double kFirstOrderMu = 0.9;

std::string_view FieldOf(Axis axis) {
  switch (axis) {
    case Axis::kTargetRms:
      return "target_rms";
    case Axis::kRepeatLambda:
      return "repeat_lambda";
    case Axis::kGateDb:
      return "gate_db";
    case Axis::kPreEmphasis:
      return "pre_emphasis";
    case Axis::kWindow:
      return "window";
    case Axis::kLagWindowHz:
      return "lag_window_hz";
    case Axis::kVoicingThreshold:
      return "voicing_threshold";
    case Axis::kVoicingEvidence:
      return "voicing_evidence";
    case Axis::kLevelAgc:
      return "level_agc";
  }
  return {};
}

// The numeric value of an axis: the real or whole-number option, or the
// pre-emphasis coefficient.
double ValueOf(const EncoderOptions& options, Axis axis) {
  switch (axis) {
    case Axis::kTargetRms:
      return options.target_rms;
    case Axis::kRepeatLambda:
      return options.repeat_lambda;
    case Axis::kGateDb:
      return options.gate_db;
    case Axis::kPreEmphasis:
      return options.pre_emphasis.mu;
    case Axis::kWindow:
      return options.window;
    case Axis::kLagWindowHz:
      return options.lag_window_hz;
    case Axis::kVoicingThreshold:
      return options.voicing_threshold;
    case Axis::kVoicingEvidence:
      return options.voicing_evidence;
    case Axis::kLevelAgc:
      return options.level_agc;
  }
  return 0.0;
}

// `options` with the axis set to `value`, limited to the option's range and
// rounded to `decimals` places (whole numbers for whole-number options).
EncoderOptions WithValue(EncoderOptions options, Axis axis, double value,
                         int decimals) {
  const encoder::EncoderOptionInfo* info =
      encoder::FindEncoderOption(FieldOf(axis));
  value = std::clamp(value, info->minimum, info->maximum);
  const double scale = std::pow(10.0, decimals);
  value = std::round(value * scale) / scale;
  switch (axis) {
    case Axis::kTargetRms:
      options.target_rms = value;
      break;
    case Axis::kRepeatLambda:
      options.repeat_lambda = value;
      break;
    case Axis::kGateDb:
      options.gate_db = value;
      break;
    case Axis::kPreEmphasis:
      options.pre_emphasis = encoder::PreEmphasis::FirstOrder(value);
      break;
    case Axis::kWindow:
      options.window = static_cast<int>(value);
      break;
    case Axis::kLagWindowHz:
      options.lag_window_hz = value;
      break;
    case Axis::kVoicingThreshold:
      options.voicing_threshold = value;
      break;
    case Axis::kVoicingEvidence:
      options.voicing_evidence = value;
      break;
    case Axis::kLevelAgc:
      options.level_agc = value;
      break;
  }
  return options;
}

// The options with the other pre-emphasis kind: the first-order filter in
// place of the chirp's, or the chirp's in place of the first-order.
EncoderOptions OtherPreEmphasis(EncoderOptions options) {
  options.pre_emphasis = options.pre_emphasis.chirp
                             ? encoder::PreEmphasis::FirstOrder(kFirstOrderMu)
                             : encoder::PreEmphasis::Chirp();
  return options;
}

// Identifies a set of options: every option's text, in EncoderOptionInfos()
// order.
std::string KeyOf(const EncoderOptions& options) {
  std::string key;
  for (const encoder::EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    key += encoder::EncoderOptionText(options, info.field);
    key += ';';
  }
  return key;
}

// Each grid option's values, the start's first.
std::vector<EncoderOptions> GridAround(const EncoderOptions& start) {
  using Variants = std::vector<EncoderOptions>;
  std::vector<Variants (*)(const EncoderOptions&)> dimensions = {
      [](const EncoderOptions& o) -> Variants {
        return {o, OtherPreEmphasis(o)};
      },
      [](const EncoderOptions& o) -> Variants {
        constexpr int kDecimals = 4;
        return {
            o,
            WithValue(o, Axis::kVoicingThreshold,
                      o.voicing_threshold - kGridVoicingThreshold, kDecimals),
            WithValue(o, Axis::kVoicingThreshold,
                      o.voicing_threshold + kGridVoicingThreshold, kDecimals)};
      },
      [](const EncoderOptions& o) -> Variants {
        constexpr int kDecimals = 2;
        return {
            o,
            WithValue(o, Axis::kVoicingEvidence,
                      o.voicing_evidence - kGridVoicingEvidence, kDecimals),
            WithValue(o, Axis::kVoicingEvidence,
                      o.voicing_evidence + kGridVoicingEvidence, kDecimals)};
      },
      [](const EncoderOptions& o) -> Variants {
        constexpr int kDecimals = 1;
        return {
            o,
            WithValue(o, Axis::kRepeatLambda,
                      o.repeat_lambda / kGridRepeatLambdaFactor, kDecimals),
            WithValue(o, Axis::kRepeatLambda,
                      o.repeat_lambda * kGridRepeatLambdaFactor, kDecimals)};
      },
      [](const EncoderOptions& o) -> Variants {
        constexpr int kDecimals = 2;
        return {o,
                WithValue(o, Axis::kTargetRms, o.target_rms - kGridTargetRms,
                          kDecimals),
                WithValue(o, Axis::kTargetRms, o.target_rms + kGridTargetRms,
                          kDecimals)};
      },
  };
  // The Cartesian product, each dimension applied to every point so far.
  std::vector<EncoderOptions> points = {start};
  for (const auto& variants_of : dimensions) {
    std::vector<EncoderOptions> next;
    for (const EncoderOptions& point : points) {
      for (const EncoderOptions& variant : variants_of(point)) {
        next.push_back(variant);
      }
    }
    points = std::move(next);
  }
  return points;
}

// One step down and one up along an axis from `options`; for the
// pre-emphasis, also the other kind.
std::vector<EncoderOptions> MovesAlong(const EncoderOptions& options,
                                       const AxisSteps& steps, double step) {
  if (steps.axis == Axis::kPreEmphasis) {
    if (options.pre_emphasis.chirp) {
      return {OtherPreEmphasis(options)};
    }
    return {WithValue(options, steps.axis, options.pre_emphasis.mu - step,
                      steps.decimals),
            WithValue(options, steps.axis, options.pre_emphasis.mu + step,
                      steps.decimals),
            OtherPreEmphasis(options)};
  }
  const double value = ValueOf(options, steps.axis);
  return {WithValue(options, steps.axis, value - step, steps.decimals),
          WithValue(options, steps.axis, value + step, steps.decimals)};
}

// True if `a` is a better choice than `b`: a higher objective, or the same
// and an earlier candidate.
bool Better(const Candidate& a, const Candidate& b) {
  if (a.objective != b.objective) {
    return a.objective > b.objective;
  }
  return a.index < b.index;
}

// What one candidate job produces.
struct Evaluation {
  Candidate candidate;
  std::vector<lpc::Frame> frames;
};

// The state shared by a batch's jobs and the thread waiting for them.
struct Batch {
  std::mutex mutex;
  std::condition_variable finished_one;
  int finished = 0;
  std::vector<Evaluation> evaluations;
};

}  // namespace

std::string_view SearchStageName(SearchStage stage) {
  switch (stage) {
    case SearchStage::kStart:
      return "start";
    case SearchStage::kGrid:
      return "grid";
    case SearchStage::kDescent:
      return "descent";
  }
  return "unknown";
}

std::string_view CandidateOutcomeName(CandidateOutcome outcome) {
  switch (outcome) {
    case CandidateOutcome::kScored:
      return "scored";
    case CandidateOutcome::kOverBudget:
      return "over budget";
    case CandidateOutcome::kOverLimit:
      return "over limit";
    case CandidateOutcome::kFailed:
      return "failed";
    case CandidateOutcome::kCancelled:
      return "cancelled";
  }
  return "unknown";
}

const Candidate* OptimiseResult::Start() const {
  return history.empty() ? nullptr : &history.front();
}

const Candidate* OptimiseResult::Best() const {
  return best.has_value() ? &history[static_cast<std::size_t>(*best)] : nullptr;
}

Optimiser::Optimiser(encoder::IEncoder& encoder,
                     quality::IQualityMetric& metric, ITaskRunner& workers)
    : encoder_(&encoder), metric_(&metric), workers_(&workers) {}

StatusOr<OptimiseResult> Optimiser::Run(const OptimiseRequest& request,
                                        const std::atomic<bool>* cancel,
                                        const ProgressCallback& progress) {
  if (Status status = encoder::ValidateEncoderOptions(request.start);
      !status.IsOk()) {
    return status;
  }
  const auto log = Logger(kLogModuleOptimiser);
  const bool higher_is_better =
      metric_->Direction() == quality::MetricDirection::kHigherIsBetter;
  const std::span<const AxisSteps> axes = kDescentAxes;
  const std::vector<EncoderOptions> grid = GridAround(request.start);
  const int expected = std::min(
      request.plan.max_evaluations,
      1 + static_cast<int>(grid.size()) +
          (request.plan.max_rounds * 2 * static_cast<int>(axes.size())));
  log->info("optimising from {} candidates at most", expected);

  OptimiseResult result;
  std::vector<std::vector<lpc::Frame>> frames;
  // The index of each set of options tried.
  std::map<std::string, int> tried;
  auto cancelled = [cancel] {
    return cancel != nullptr && cancel->load(std::memory_order_relaxed);
  };

  // What the candidates are scored against: the source squelched as the
  // encoder squelches it (no axis changes the squelch).
  const std::vector<double> reference =
      encoder::SquelchedSource(request.source, request.start);

  // Encodes, checks and measures one candidate (on a worker).
  auto evaluate = [this, &request, &reference, higher_is_better,
                   cancelled](Candidate candidate) -> Evaluation {
    Evaluation evaluation{.candidate = std::move(candidate), .frames = {}};
    Candidate& c = evaluation.candidate;
    if (cancelled()) {
      c.outcome = CandidateOutcome::kCancelled;
      return evaluation;
    }
    StatusOr<encoder::EncodeResult> encoding =
        encoder_->Encode(request.source, kSampleRate, c.options);
    if (!encoding.IsOk()) {
      c.outcome = CandidateOutcome::kFailed;
      c.message = encoding.GetStatus().Message();
      return evaluation;
    }
    const lpc::FrameStatistics stats = lpc::SummariseFrames(encoding->frames);
    c.bytes = stats.bytes;
    c.bit_rate = stats.BitRate();
    if (request.limits.max_bytes.has_value() &&
        c.bytes > *request.limits.max_bytes) {
      c.outcome = CandidateOutcome::kOverLimit;
      c.message = fmt::format("{} bytes, over the limit of {}", c.bytes,
                              *request.limits.max_bytes);
      return evaluation;
    }
    if (request.budget) {
      if (Status status = request.budget(c.bytes); !status.IsOk()) {
        c.outcome = CandidateOutcome::kOverBudget;
        c.message = status.Message();
        return evaluation;
      }
    }
    StatusOr<double> score = metric_->Measure(reference, *encoding);
    if (!score.IsOk()) {
      c.outcome = CandidateOutcome::kFailed;
      c.message = score.GetStatus().Message();
      return evaluation;
    }
    if (!std::isfinite(*score)) {
      c.outcome = CandidateOutcome::kFailed;
      c.message = "the quality measure gave no number";
      return evaluation;
    }
    c.outcome = CandidateOutcome::kScored;
    c.score = *score;
    c.objective =
        (higher_is_better ? c.score : -c.score) -
        (request.limits.bit_rate_weight * c.bit_rate / kBitsPerKilobit);
    evaluation.frames = std::move(encoding->frames);
    return evaluation;
  };

  // Evaluates the options not tried before, in parallel, and adds them to
  // the history in order. Returns the index of each option given (tried
  // before or now), or nothing for those past the evaluation limit.
  auto run_batch = [&](const std::vector<EncoderOptions>& options,
                       SearchStage stage) {
    std::vector<std::optional<int>> indices(options.size());
    std::vector<Candidate> fresh;
    for (std::size_t i = 0; i < options.size(); ++i) {
      const std::string key = KeyOf(options[i]);
      if (const auto found = tried.find(key); found != tried.end()) {
        indices[i] = found->second;
        continue;
      }
      const int index = static_cast<int>(result.history.size() + fresh.size());
      if (index >= request.plan.max_evaluations) {
        continue;
      }
      tried.emplace(key, index);
      indices[i] = index;
      Candidate candidate;
      candidate.index = index;
      candidate.stage = stage;
      candidate.options = options[i];
      fresh.push_back(std::move(candidate));
    }
    if (fresh.empty()) {
      return indices;
    }
    auto batch = std::make_shared<Batch>();
    batch->evaluations.resize(fresh.size());
    const int count = static_cast<int>(fresh.size());
    for (int i = 0; i < count; ++i) {
      workers_->Post([batch, i, &evaluate,
                      candidate = std::move(
                          fresh[static_cast<std::size_t>(i)])]() mutable {
        Evaluation evaluation = evaluate(std::move(candidate));
        const std::scoped_lock lock(batch->mutex);
        batch->evaluations[static_cast<std::size_t>(i)] = std::move(evaluation);
        ++batch->finished;
        // Notified under the lock: the waiting thread may return, and
        // release the batch, as soon as it sees the last one finish.
        batch->finished_one.notify_one();
      });
    }
    const int before = static_cast<int>(result.history.size());
    int reported = 0;
    std::unique_lock lock(batch->mutex);
    while (reported < count) {
      batch->finished_one.wait(lock,
                               [&] { return batch->finished > reported; });
      const int finished = batch->finished;
      lock.unlock();
      if (progress) {
        for (int k = reported + 1; k <= finished; ++k) {
          progress(result, before + k, expected);
        }
      }
      reported = finished;
      lock.lock();
    }
    lock.unlock();
    for (Evaluation& evaluation : batch->evaluations) {
      log->trace("candidate {}: {} {} bytes score {}",
                 evaluation.candidate.index,
                 CandidateOutcomeName(evaluation.candidate.outcome),
                 evaluation.candidate.bytes, evaluation.candidate.score);
      result.history.push_back(std::move(evaluation.candidate));
      frames.push_back(std::move(evaluation.frames));
    }
    return indices;
  };

  // The best scored candidate among `indices`, or none.
  auto best_of = [&result](const std::vector<std::optional<int>>& indices) {
    std::optional<int> best;
    for (const std::optional<int>& index : indices) {
      if (!index.has_value()) {
        continue;
      }
      const Candidate& c = result.history[static_cast<std::size_t>(*index)];
      if (c.IsScored() &&
          (!best.has_value() ||
           Better(c, result.history[static_cast<std::size_t>(*best)]))) {
        best = index;
      }
    }
    return best;
  };

  run_batch({request.start}, SearchStage::kStart);
  // The grid's first point is the start, so its best is the best so far.
  std::optional<int> current = best_of(run_batch(grid, SearchStage::kGrid));

  // Coordinate descent from the best so far.
  std::vector<double> steps;
  steps.reserve(axes.size());
  for (const AxisSteps& axis : axes) {
    steps.push_back(axis.step);
  }
  for (int round = 0;
       current.has_value() && round < request.plan.max_rounds && !cancelled() &&
       std::cmp_less(result.history.size(), request.plan.max_evaluations);
       ++round) {
    const EncoderOptions here =
        result.history[static_cast<std::size_t>(*current)].options;
    std::vector<EncoderOptions> moves;
    for (std::size_t a = 0; a < axes.size(); ++a) {
      for (const EncoderOptions& moved : MovesAlong(here, axes[a], steps[a])) {
        if (KeyOf(moved) != KeyOf(here)) {
          moves.push_back(moved);
        }
      }
    }
    const std::optional<int> best_move =
        best_of(run_batch(moves, SearchStage::kDescent));
    const Candidate& now = result.history[static_cast<std::size_t>(*current)];
    if (best_move.has_value() &&
        result.history[static_cast<std::size_t>(*best_move)].objective >
            now.objective) {
      log->debug("round {}: moved to candidate {}", round, *best_move);
      current = best_move;
      continue;
    }
    bool refined = false;
    for (std::size_t a = 0; a < axes.size(); ++a) {
      if (steps[a] > axes[a].min_step) {
        steps[a] = std::max(steps[a] / 2.0, axes[a].min_step);
        refined = true;
      }
    }
    if (!refined) {
      log->debug("round {}: no better move at the smallest steps", round);
      break;
    }
    log->debug("round {}: no better move; halved the steps", round);
  }

  result.cancelled = cancelled();
  // The best of everything: the descent only moves to better candidates,
  // so this is where it ended, but the history decides.
  std::vector<std::optional<int>> all;
  all.reserve(result.history.size());
  for (const Candidate& c : result.history) {
    all.emplace_back(c.index);
  }
  result.best = best_of(all);
  if (result.best.has_value()) {
    result.best_frames = frames[static_cast<std::size_t>(*result.best)];
    log->info("best of {} candidates: {} ({} bytes)", result.history.size(),
              result.Best()->score, result.Best()->bytes);
  } else {
    log->info("no candidate of {} within the limits", result.history.size());
  }
  return result;
}

}  // namespace speak5220::core::optimiser
