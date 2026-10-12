/*
 * File:        optimiser_test.cpp
 * Module:      optimiser tests
 * Purpose:     Unit tests of the optimiser's search with a mocked encoder
 *              and metric on an inline task runner
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/optimiser/optimiser.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/quality/quality_metric.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"
#include "support/quality_mocks.h"

namespace speak5220::core::optimiser {
namespace {

using encoder::EncoderOptions;
using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;

// A pretend encoder and metric: the encoding's size and score are
// functions of the options. With an inline runner each candidate is
// encoded and then measured before the next, so the metric scores the
// options the encoder was last given.
class FakeSearch {
 public:
  FakeSearch() {
    ON_CALL(encoder_, Encode(_, _, _))
        .WillByDefault(
            [this](std::span<const double>, int, const EncoderOptions& options)
                -> StatusOr<encoder::EncodeResult> {
              ++encodings;
              last_ = options;
              if (fail && fail(options)) {
                return InternalError("the encoder gave up");
              }
              return encoder::EncodeResult{
                  .frames = frames ? frames(options)
                                   : testing::FramesPackingTo(bytes(options)),
                  .details = std::nullopt};
            });
    ON_CALL(metric_, Measure(_, _))
        .WillByDefault(
            [this](std::span<const double>,
                   const encoder::EncodeResult&) -> StatusOr<double> {
              ++measurements;
              return score(last_);
            });
    ON_CALL(metric_, Direction())
        .WillByDefault(Return(quality::MetricDirection::kHigherIsBetter));
  }

  StatusOr<OptimiseResult> Run(const OptimiseRequest& request,
                               const std::atomic<bool>* cancel = nullptr,
                               const ProgressCallback& progress = {}) {
    Optimiser optimiser(encoder_, metric_, runner_);
    return optimiser.Run(request, cancel, progress);
  }

  NiceMock<testing::MockQualityMetric>& Metric() { return metric_; }

  std::function<double(const EncoderOptions&)> score =
      [](const EncoderOptions&) { return 2.0; };
  std::function<int(const EncoderOptions&)> bytes = [](const EncoderOptions&) {
    return 100;
  };
  // If set, the frames, in place of FramesPackingTo(bytes()).
  std::function<std::vector<lpc::Frame>(const EncoderOptions&)> frames;
  std::function<bool(const EncoderOptions&)> fail;
  int encodings = 0;
  int measurements = 0;

 private:
  NiceMock<testing::MockEncoder> encoder_;
  NiceMock<testing::MockQualityMetric> metric_;
  InlineTaskRunner runner_;
  EncoderOptions last_;
};

const std::vector<double> kSource(800, 0.1);

OptimiseRequest RequestFrom(const EncoderOptions& start = {}) {
  return {.source = kSource,
          .start = start,
          .limits = {},
          .budget = {},
          .plan = {}};
}

// Every option's text, to tell sets of options apart.
std::string KeyOf(const EncoderOptions& options) {
  std::string key;
  for (const encoder::EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    key += encoder::EncoderOptionText(options, info.field) + ";";
  }
  return key;
}

TEST(OptimiserTest, TriesTheStartThenTheGridAroundIt) {
  FakeSearch search;
  const StatusOr<OptimiseResult> result = search.Run(RequestFrom());
  ASSERT_TRUE(result.IsOk());
  ASSERT_FALSE(result->history.empty());
  EXPECT_EQ(result->Start()->stage, SearchStage::kStart);
  EXPECT_EQ(result->Start()->options, EncoderOptions{});
  // 2 pre-emphases x 3 thresholds x 3 evidences x 3 trade-offs x 3
  // targets, less the start.
  constexpr int kGrid = (2 * 3 * 3 * 3 * 3) - 1;
  for (int i = 1; i <= kGrid; ++i) {
    EXPECT_EQ(result->history[static_cast<std::size_t>(i)].stage,
              SearchStage::kGrid);
  }
  std::set<double> targets;
  for (int i = 0; i <= kGrid; ++i) {
    targets.insert(
        result->history[static_cast<std::size_t>(i)].options.target_rms);
  }
  EXPECT_EQ(targets, (std::set<double>{20.0, 24.0, 28.0}));
}

// The squelch changes what encodings are scored against, so it is kept
// from the start, and every candidate is scored against the source as
// the start squelches it.
TEST(OptimiserTest, KeepsTheSquelchAndScoresAgainstTheSquelchedSource) {
  FakeSearch search;
  // Loud, then 60 dB quieter.
  std::vector<double> source(8000, 0.5);
  std::fill(source.begin() + 4000, source.end(), 0.0005);
  EncoderOptions start;
  start.squelch_db = 30.0;
  const std::vector<double> squelched = encoder::SquelchedSource(source, start);
  int unsquelched = 0;
  ON_CALL(search.Metric(), Measure(_, _))
      .WillByDefault([&](std::span<const double> reference,
                         const encoder::EncodeResult&) -> StatusOr<double> {
        if (!std::ranges::equal(reference, squelched)) {
          ++unsquelched;
        }
        return 2.0;
      });
  OptimiseRequest request = RequestFrom(start);
  request.source = source;
  request.plan.max_evaluations = 30;
  const StatusOr<OptimiseResult> result = search.Run(request);
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(unsquelched, 0);
  for (const Candidate& c : result->history) {
    EXPECT_EQ(c.options.squelch_db, 30.0);
  }
}

TEST(OptimiserTest, DescendsToTheBestOptions) {
  FakeSearch search;
  // Best at a target RMS of 23 and a silence gate of 52 dB, neither on
  // the grid.
  search.score = [](const EncoderOptions& o) {
    return 3.0 - (0.01 * std::pow(o.target_rms - 23.0, 2)) -
           (0.001 * std::pow(o.gate_db - 52.0, 2));
  };
  const StatusOr<OptimiseResult> result = search.Run(RequestFrom());
  ASSERT_TRUE(result.IsOk());
  ASSERT_NE(result->Best(), nullptr);
  EXPECT_NEAR(result->Best()->options.target_rms, 23.0, 0.5);
  EXPECT_NEAR(result->Best()->options.gate_db, 52.0, 1.25);
  EXPECT_EQ(result->Best()->stage, SearchStage::kDescent);
  EXPECT_FALSE(result->best_frames.empty());
  EXPECT_FALSE(result->cancelled);
}

TEST(OptimiserTest, TiesGoToTheLowestCandidateIndex) {
  FakeSearch search;
  const StatusOr<OptimiseResult> result = search.Run(RequestFrom());
  ASSERT_TRUE(result.IsOk());
  ASSERT_TRUE(result->best.has_value());
  EXPECT_EQ(*result->best, 0);
}

TEST(OptimiserTest, NeverTriesTheSameOptionsTwice) {
  FakeSearch search;
  search.score = [](const EncoderOptions& o) { return o.target_rms; };
  const StatusOr<OptimiseResult> result = search.Run(RequestFrom());
  ASSERT_TRUE(result.IsOk());
  std::set<std::string> keys;
  for (const Candidate& c : result->history) {
    EXPECT_TRUE(keys.insert(KeyOf(c.options)).second) << c.index;
  }
  EXPECT_EQ(search.encodings, static_cast<int>(result->history.size()));
}

TEST(OptimiserTest, RejectsCandidateThatExceedsProjectBudget) {
  FakeSearch search;
  // Louder is better and bigger; the budget allows up to 110 bytes.
  search.score = [](const EncoderOptions& o) { return o.target_rms; };
  search.bytes = [](const EncoderOptions& o) {
    return static_cast<int>(o.target_rms * 5.0);
  };
  OptimiseRequest request = RequestFrom();
  request.budget = [](int bytes) {
    return bytes <= 110 ? OkStatus() : ResourceExhaustedError("too big");
  };
  const StatusOr<OptimiseResult> result = search.Run(request);
  ASSERT_TRUE(result.IsOk());
  ASSERT_NE(result->Best(), nullptr);
  EXPECT_LE(result->Best()->bytes, 110);
  EXPECT_NEAR(result->Best()->options.target_rms, 22.0, 0.5);
  int over = 0;
  for (const Candidate& c : result->history) {
    if (c.bytes > 110) {
      EXPECT_EQ(c.outcome, CandidateOutcome::kOverBudget);
      EXPECT_EQ(c.message, "too big");
      ++over;
    }
  }
  EXPECT_GT(over, 0);
  // Candidates over the budget are not measured.
  EXPECT_EQ(search.measurements,
            static_cast<int>(result->history.size()) - over);
}

TEST(OptimiserTest, KeepsWithinThePerPhraseByteLimit) {
  FakeSearch search;
  search.score = [](const EncoderOptions& o) { return o.target_rms; };
  search.bytes = [](const EncoderOptions& o) {
    return static_cast<int>(o.target_rms * 5.0);
  };
  EncoderOptions start;
  start.target_rms = 20.0;
  OptimiseRequest request = RequestFrom(start);
  request.limits.max_bytes = 90;
  const StatusOr<OptimiseResult> result = search.Run(request);
  ASSERT_TRUE(result.IsOk());
  ASSERT_NE(result->Best(), nullptr);
  EXPECT_LE(result->Best()->bytes, 90);
  for (const Candidate& c : result->history) {
    if (c.bytes > 90) {
      EXPECT_EQ(c.outcome, CandidateOutcome::kOverLimit);
    }
  }
}

TEST(OptimiserTest, BitRateWeightTradesScoreForBytes) {
  FakeSearch search;
  // Ten frames, one more voiced (46 bits more, 0.184 kbit/s) for each 4
  // codes of target RMS, which also add 0.04 to the score.
  search.score = [](const EncoderOptions& o) { return 0.01 * o.target_rms; };
  search.frames = [](const EncoderOptions& o) {
    constexpr int kFrames = 10;
    const auto voiced = static_cast<int>(std::lround(o.target_rms / 4.0));
    std::vector<lpc::Frame> frames;
    frames.reserve(kFrames + 1);
    for (int i = 0; i < kFrames; ++i) {
      frames.push_back(
          i < voiced ? lpc::Frame::Voiced(5, 20, {1, 1, 1, 1, 1, 1, 1, 1, 1, 1})
                     : lpc::Frame::Silence());
    }
    frames.push_back(lpc::Frame::Stop());
    return frames;
  };
  OptimiseRequest request = RequestFrom();
  const StatusOr<OptimiseResult> plain = search.Run(request);
  request.limits.bit_rate_weight = 1.0;
  const StatusOr<OptimiseResult> weighted = search.Run(request);
  ASSERT_TRUE(plain.IsOk());
  ASSERT_TRUE(weighted.IsOk());
  EXPECT_LT(weighted->Best()->options.target_rms,
            plain->Best()->options.target_rms);
  const Candidate& best = *weighted->Best();
  EXPECT_DOUBLE_EQ(best.objective, best.score - (best.bit_rate / 1000.0));
}

TEST(OptimiserTest, MinimisesAMetricWhereLowerIsBetter) {
  FakeSearch search;
  ON_CALL(search.Metric(), Direction())
      .WillByDefault(Return(quality::MetricDirection::kLowerIsBetter));
  search.score = [](const EncoderOptions& o) { return o.target_rms; };
  const StatusOr<OptimiseResult> result = search.Run(RequestFrom());
  ASSERT_TRUE(result.IsOk());
  ASSERT_NE(result->Best(), nullptr);
  EXPECT_LT(result->Best()->options.target_rms, 16.0);
}

TEST(OptimiserTest, FailedCandidatesAreNeverChosen) {
  FakeSearch search;
  search.score = [](const EncoderOptions& o) { return o.target_rms; };
  search.fail = [](const EncoderOptions& o) { return o.target_rms > 20.0; };
  const StatusOr<OptimiseResult> result = search.Run(RequestFrom());
  ASSERT_TRUE(result.IsOk());
  ASSERT_NE(result->Best(), nullptr);
  EXPECT_LE(result->Best()->options.target_rms, 20.0);
  const auto failed =
      std::ranges::count_if(result->history, [](const Candidate& c) {
        return c.outcome == CandidateOutcome::kFailed &&
               c.message == "the encoder gave up";
      });
  EXPECT_GT(failed, 0);
}

TEST(OptimiserTest, NoCandidateWithinTheLimitHasNoBest) {
  FakeSearch search;
  OptimiseRequest request = RequestFrom();
  request.limits.max_bytes = 10;
  const StatusOr<OptimiseResult> result = search.Run(request);
  ASSERT_TRUE(result.IsOk());
  EXPECT_FALSE(result->best.has_value());
  EXPECT_EQ(result->Best(), nullptr);
  EXPECT_TRUE(result->best_frames.empty());
  EXPECT_EQ(search.measurements, 0);
}

TEST(OptimiserTest, StopsAtTheEvaluationLimit) {
  FakeSearch search;
  search.score = [](const EncoderOptions& o) { return o.target_rms; };
  OptimiseRequest request = RequestFrom();
  request.plan.max_evaluations = 20;
  const StatusOr<OptimiseResult> result = search.Run(request);
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(result->history.size(), 20U);
  EXPECT_EQ(search.encodings, 20);
}

TEST(OptimiserTest, CancellingKeepsTheBestSoFar) {
  FakeSearch search;
  std::atomic<bool> cancel = false;
  // Cancelled while the 30th candidate is measured.
  search.score = [&](const EncoderOptions& o) {
    if (search.encodings == 30) {
      cancel = true;
    }
    return o.target_rms;
  };
  int last_evaluated = 0;
  const StatusOr<OptimiseResult> result = search.Run(
      RequestFrom(), &cancel, [&](const OptimiseResult&, int evaluated, int) {
        last_evaluated = evaluated;
      });
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(result->cancelled);
  EXPECT_EQ(search.encodings, 30);
  // The rest of the grid was cancelled, and there was no descent.
  for (const Candidate& c : result->history) {
    EXPECT_NE(c.stage, SearchStage::kDescent);
    EXPECT_EQ(c.outcome, c.index < 30 ? CandidateOutcome::kScored
                                      : CandidateOutcome::kCancelled);
  }
  ASSERT_NE(result->Best(), nullptr);
  EXPECT_LT(result->Best()->index, 30);
  EXPECT_EQ(last_evaluated, static_cast<int>(result->history.size()));
}

TEST(OptimiserTest, ReportsProgressUpToTheExpectedCount) {
  FakeSearch search;
  std::vector<int> evaluated;
  int expected = 0;
  const StatusOr<OptimiseResult> result = search.Run(
      RequestFrom(), nullptr, [&](const OptimiseResult&, int done, int most) {
        evaluated.push_back(done);
        expected = most;
      });
  ASSERT_TRUE(result.IsOk());
  ASSERT_FALSE(evaluated.empty());
  EXPECT_TRUE(std::ranges::is_sorted(evaluated));
  EXPECT_EQ(evaluated.front(), 1);
  EXPECT_EQ(evaluated.back(), static_cast<int>(result->history.size()));
  EXPECT_LE(evaluated.back(), expected);
  EXPECT_LE(expected, SearchPlan{}.max_evaluations);
}

TEST(OptimiserTest, RefusesStartOptionsOutOfRange) {
  FakeSearch search;
  EncoderOptions start;
  start.window = 1;
  EXPECT_EQ(search.Run(RequestFrom(start)).GetStatus().Code(),
            StatusCode::kOutOfRange);
  EXPECT_EQ(search.encodings, 0);
}

TEST(OptimiserTest, OptionsStayInRangeAndShort) {
  FakeSearch search;
  // Pushes every option towards its upper end.
  search.score = [](const EncoderOptions& o) {
    return o.target_rms + o.gate_db + o.voicing_threshold + o.level_agc +
           o.window + o.lag_window_hz;
  };
  const StatusOr<OptimiseResult> result = search.Run(RequestFrom());
  ASSERT_TRUE(result.IsOk());
  for (const Candidate& c : result->history) {
    EXPECT_TRUE(encoder::ValidateEncoderOptions(c.options).IsOk()) << c.index;
    EXPECT_LE(encoder::EncoderOptionText(c.options, "voicing_threshold").size(),
              6U)
        << encoder::EncoderOptionText(c.options, "voicing_threshold");
  }
}

TEST(OptimiserTest, NamesStagesAndOutcomes) {
  EXPECT_EQ(SearchStageName(SearchStage::kGrid), "grid");
  EXPECT_EQ(CandidateOutcomeName(CandidateOutcome::kOverBudget), "over budget");
}

}  // namespace
}  // namespace speak5220::core::optimiser
