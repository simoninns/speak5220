/*
 * File:        noise_gate_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of the noise gate: what it mutes, turns down and
 *              keeps, how fast it closes and opens, and the stretches it
 *              reports
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/noise_gate.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

constexpr int kRate = 8000;
// Half a second at 8 kHz.
constexpr std::size_t kStretch = 4000;
// 60 dB below the loud level.
constexpr double kQuiet = 0.001;
// A gate 30 dB down that closes over 100 ms (800 samples).
constexpr double kBelowDb = 30.0;
constexpr double kCloseMs = 100.0;
constexpr std::size_t kCloseSamples = 800;
// Opening takes a quarter of that.
constexpr std::size_t kOpenSamples = 200;
// Half the 20 ms level window: how far the level sees ahead and behind.
constexpr std::size_t kHalfWindow = 80;

// Loud (1.0), then `quiet`, then loud again, each half a second. The
// samples are constant, so the output over the input is the gain.
std::vector<double> LoudQuietLoud(double quiet = kQuiet) {
  std::vector<double> x(3 * kStretch, 1.0);
  for (std::size_t i = kStretch; i < 2 * kStretch; ++i) {
    x[i] = quiet;
  }
  return x;
}

TEST(NoiseGateTest, MutesStretchesWellBelowTheThreshold) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> y = NoiseGate(x, kRate, kBelowDb, kCloseMs);
  ASSERT_EQ(y.size(), x.size());
  EXPECT_EQ(y[kStretch + (kStretch / 2)], 0.0);
  EXPECT_EQ(y[kStretch / 2], 1.0);
  EXPECT_EQ(y[(2 * kStretch) + (kStretch / 2)], 1.0);
}

TEST(NoiseGateTest, KeepsStretchesAboveTheThreshold) {
  const std::vector<double> x = LoudQuietLoud();
  EXPECT_EQ(NoiseGate(x, kRate, 70.0, kCloseMs), x);
}

// The knee is soft: sound 3 dB under the threshold is turned down by 9 dB,
// not muted.
TEST(NoiseGateTest, TurnsSoundJustUnderTheThresholdDownALittle) {
  const double quiet = std::pow(10.0, -(kBelowDb + 3.0) / 20.0);
  const std::vector<double> x = LoudQuietLoud(quiet);
  const std::vector<double> gain = NoiseGateGain(x, kRate, kBelowDb, kCloseMs);
  EXPECT_NEAR(gain[kStretch + (kStretch / 2)], std::pow(10.0, -9.0 / 20.0),
              1e-9);
}

// After the sound the gate fades down over its speed, and a faster gate
// closes sooner.
TEST(NoiseGateTest, ClosesOverItsSpeedAfterTheSound) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> gain = NoiseGateGain(x, kRate, kBelowDb, kCloseMs);
  const std::size_t fade_start = kStretch + kHalfWindow;
  for (std::size_t i = fade_start + 1; i < fade_start + kCloseSamples; ++i) {
    ASSERT_LT(gain[i], gain[i - 1]) << i;
    ASSERT_GT(gain[i], 0.0) << i;
  }
  EXPECT_EQ(gain[fade_start + kCloseSamples + 1], 0.0);

  const std::vector<double> fast = NoiseGateGain(x, kRate, kBelowDb, 20.0);
  EXPECT_GT(gain[fade_start + 200], 0.0);
  EXPECT_EQ(fast[fade_start + 200], 0.0);
}

// The gate opens before the sound, so the gate is open when it starts.
TEST(NoiseGateTest, OpensJustBeforeTheSound) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> gain = NoiseGateGain(x, kRate, kBelowDb, kCloseMs);
  const std::size_t onset = 2 * kStretch;
  EXPECT_EQ(gain[onset], 1.0);
  const std::size_t rise_end = onset - kHalfWindow;
  for (std::size_t i = rise_end - kOpenSamples + 1; i < rise_end; ++i) {
    ASSERT_GT(gain[i], gain[i - 1]) << i;
  }
  EXPECT_EQ(gain[rise_end - kOpenSamples - 1], 0.0);
}

TEST(NoiseGateTest, SpansAreWhereTheGainIsHalvedOrLess) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> gain = NoiseGateGain(x, kRate, kBelowDb, kCloseMs);
  const std::vector<SampleSpan> spans =
      GatedSpans(x, kRate, kBelowDb, kCloseMs);
  ASSERT_EQ(spans.size(), 1U);
  EXPECT_GT(spans[0].begin, kStretch);
  EXPECT_LT(spans[0].end, 2 * kStretch);
  for (std::size_t i = 0; i < gain.size(); ++i) {
    const bool in_span = i >= spans[0].begin && i < spans[0].end;
    ASSERT_EQ(in_span, gain[i] < kGatedGain) << i;
  }
  EXPECT_TRUE(GatedSpans(x, kRate, 70.0, kCloseMs).empty());
}

TEST(NoiseGateTest, SilenceAndNothingComeBackUnchanged) {
  const std::vector<double> silence(1000, 0.0);
  EXPECT_EQ(NoiseGate(silence, kRate, kBelowDb, kCloseMs), silence);
  EXPECT_TRUE(
      NoiseGate(std::vector<double>{}, kRate, kBelowDb, kCloseMs).empty());
  EXPECT_TRUE(GatedSpans(silence, kRate, kBelowDb, kCloseMs).empty());
}

}  // namespace
}  // namespace speak5220::core::dsp
