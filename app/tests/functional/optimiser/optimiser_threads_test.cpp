/*
 * File:        optimiser_threads_test.cpp
 * Module:      optimiser tests
 * Purpose:     Checks that the optimiser finds the same result on 1 and 8
 *              threads, with the real encoder, emulator and PESQ
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <cstddef>
#include <memory>

#include "core/encoder/encoder.h"
#include "core/optimiser/optimiser.h"
#include "core/quality/pesq_metric.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

namespace optimiser = core::optimiser;

// A short search over a short corpus file keeps the test to seconds.
constexpr int kEvaluations = 60;

core::StatusOr<optimiser::OptimiseResult> OptimiseOn(int threads,
                                                     const WavSamples& wav) {
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::quality::PesqMetric metric(synth);
  core::ThreadPoolTaskRunner workers(threads);
  optimiser::Optimiser search(*encoder, metric, workers);
  return search.Run({.source = wav.samples,
                     .start = {},
                     .limits = {},
                     .budget = {},
                     .plan = {.max_evaluations = kEvaluations}});
}

// TESTING.md, "Determinism requirements": the result must not depend on
// the thread count.
TEST(OptimiserThreadsTest, FindsTheSameResultOnOneAndEightThreads) {
  const auto wav = ReadWavSamples(EncoderGoldenDir() / "inputs/w07_joe.wav");
  ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
  const auto one = OptimiseOn(1, *wav);
  const auto eight = OptimiseOn(8, *wav);
  ASSERT_TRUE(one.IsOk()) << one.GetStatus().ToString();
  ASSERT_TRUE(eight.IsOk()) << eight.GetStatus().ToString();
  ASSERT_EQ(one->history.size(), eight->history.size());
  for (std::size_t i = 0; i < one->history.size(); ++i) {
    const optimiser::Candidate& a = one->history[i];
    const optimiser::Candidate& b = eight->history[i];
    EXPECT_EQ(a.options, b.options) << i;
    EXPECT_EQ(a.outcome, b.outcome) << i;
    EXPECT_EQ(a.bytes, b.bytes) << i;
    // Bit for bit: the same inputs through the same code.
    EXPECT_EQ(a.score, b.score) << i;
  }
  ASSERT_TRUE(one->best.has_value());
  EXPECT_EQ(one->best, eight->best);
  EXPECT_EQ(one->best_frames, eight->best_frames);
  EXPECT_GE(one->Best()->score, one->Start()->score);
}

}  // namespace
}  // namespace speak5220::testing
