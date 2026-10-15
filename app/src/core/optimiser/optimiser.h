/*
 * File:        optimiser.h
 * Module:      optimiser
 * Purpose:     Searches a phrase's encoder options for the best quality
 *              within the project budget and an optional byte limit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_OPTIMISER_OPTIMISER_H_
#define SPEAK5220_CORE_OPTIMISER_OPTIMISER_H_

#include <atomic>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/quality/quality_metric.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"

namespace speak5220::core::optimiser {

// Where in the search a candidate was proposed.
enum class SearchStage {
  // The options the search started from.
  kStart,
  // The coarse grid around them.
  kGrid,
  // A step of the coordinate descent.
  kDescent,
};

// What became of a candidate.
enum class CandidateOutcome {
  // Encoded within the limits and measured.
  kScored,
  // Its frames would take the project past its PHROM budget.
  kOverBudget,
  // Its frames are more bytes than the per-phrase limit.
  kOverLimit,
  // The encoder or the metric failed.
  kFailed,
  // The search was cancelled before it ran.
  kCancelled,
};

// "start", "grid" or "descent".
std::string_view SearchStageName(SearchStage stage);
// "scored", "over budget", "over limit", "failed" or "cancelled".
std::string_view CandidateOutcomeName(CandidateOutcome outcome);

// One set of options tried. A plain value type.
struct Candidate {
  // The order of evaluation, from 0 (the start).
  int index = 0;
  SearchStage stage = SearchStage::kStart;
  encoder::EncoderOptions options;
  CandidateOutcome outcome = CandidateOutcome::kCancelled;
  // The metric's value (the overall quality score), when scored.
  double score = 0.0;
  // The packed bytes (PackedSize()) and bits per second, when encoded.
  int bytes = 0;
  double bit_rate = 0.0;
  // What the search maximises, when scored (OptimiseLimits).
  double objective = 0.0;
  // Why it was refused or failed.
  std::string message;

  bool IsScored() const { return outcome == CandidateOutcome::kScored; }
};

// What the best encoding must satisfy, or trade. A
// plain value type.
struct OptimiseLimits {
  // At most this many bytes for the phrase; none for no limit.
  std::optional<int> max_bytes;
  // The objective is the score less this much per kbit/s, so a smaller
  // encoding can win (0: the score alone).
  double bit_rate_weight = 0.0;
};

// How far the search may go.
struct SearchPlan {
  // Evaluations in all, the start and the grid included.
  int max_evaluations = 400;
  // Rounds of the coordinate descent.
  int max_rounds = 10;
};

// OK if the phrase may be this many packed bytes
// without taking the project past its budget (project::PhraseBudgetCheck).
// Called from the worker threads; must be thread-safe.
using BudgetCheck = std::function<Status(int packed_bytes)>;

// What to optimise.
struct OptimiseRequest {
  // The phrase's 8 kHz samples, -1..1, as given to the encoder. Must
  // outlive Run().
  std::span<const double> source;
  // The options to start from: the phrase's current ones.
  encoder::EncoderOptions start;
  OptimiseLimits limits;
  // No budget when empty.
  BudgetCheck budget;
  SearchPlan plan;
};

// What the search found.
struct OptimiseResult {
  // Every candidate, in index order; the first is the start.
  std::vector<Candidate> history;
  // The index of the best scored candidate, or none if no candidate was
  // within the limits.
  std::optional<int> best;
  // The best candidate's frames, ending in a stop.
  std::vector<lpc::Frame> best_frames;
  // True if the search was cancelled; `best` is the best found so far.
  bool cancelled = false;

  // The start candidate, or nullptr before it is evaluated.
  const Candidate* Start() const;
  // The best candidate, or nullptr.
  const Candidate* Best() const;
};

// Called on the thread running the search after each candidate finishes:
// the result so far (whose history grows a batch at a time), the
// evaluations finished, and the most the search will make.
using ProgressCallback = std::function<void(const OptimiseResult& so_far,
                                            int evaluated, int expected)>;

// Optimise: searches the options in EncoderOptions for the
// encoding that maximises the metric (or the metric less a bit-rate cost),
// within the project budget and a per-phrase byte limit.
//
//  1. The start, then a coarse grid around it over the options with the
//     most effect: voicing threshold (+-0.05), voicing evidence (+-2),
//     repeat trade-off (x0.5, x2) and the target RMS (+-4).
//  2. Coordinate descent from the best so far: each round tries one step
//     down and one up along every searched option (target RMS, repeat
//     trade-off, silence gate, window, lag window, voicing threshold and
//     evidence, slow gain control), and moves to the best
//     if it improves on the current options; otherwise the steps are
//     halved, and the search ends when they reach their smallest.
//
// The preprocessing (options.preprocess) is never searched: it changes
// what the encodings are scored against, so scores with different ones do
// not compare. Every candidate keeps the start's, and is scored against
// the source preprocessed by it (preprocess::Preprocess()). Nor is
// voicing_min_run, or the resonance hold.
//
// Candidates over the budget or the limit are kept in the history but
// never chosen; they are not measured. Options already tried are not
// tried again. Each batch (the grid, a round) is evaluated in parallel on
// the workers, one candidate per job, and reduced in index order: the
// best has the highest objective, ties going to the lowest index, so the
// result does not depend on the thread count (TESTING.md).
//
// Thread-safe if the encoder, metric and runner are; the collaborators
// must outlive the optimiser.
class Optimiser {
 public:
  Optimiser(encoder::IEncoder& encoder, quality::IQualityMetric& metric,
            ITaskRunner& workers);

  // Runs the search to the end, or until `cancel` is set (checked before
  // each candidate). Blocks: it waits for the workers' jobs, so it must not
  // run as one of them. Fails for start options out of range. With an
  // InlineTaskRunner it runs entirely on the caller's thread.
  StatusOr<OptimiseResult> Run(const OptimiseRequest& request,
                               const std::atomic<bool>* cancel = nullptr,
                               const ProgressCallback& progress = {});

 private:
  encoder::IEncoder* encoder_;
  quality::IQualityMetric* metric_;
  ITaskRunner* workers_;
};

}  // namespace speak5220::core::optimiser

#endif  // SPEAK5220_CORE_OPTIMISER_OPTIMISER_H_
