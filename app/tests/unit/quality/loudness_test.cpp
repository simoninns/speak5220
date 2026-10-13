/*
 * File:        loudness_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the loudness figures: peak and clipping,
 *              sounding RMS, the BBC Micro high-pass and the A-weighting
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/loudness.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "core/lpc/frame.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::quality {
namespace {

using testing::kNeutralVoicedK;

// The steady-state gain in dB of `filter` at `hz`, from the RMS of a sine
// after its first half second.
template <typename Filter>
double GainDb(Filter filter, double hz) {
  constexpr std::size_t kRate = 8000;
  std::vector<double> x(2 * kRate);
  for (std::size_t n = 0; n < x.size(); ++n) {
    x[n] = std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(n) /
                    static_cast<double>(kRate));
  }
  const std::vector<double> y = filter(x);
  double in = 0.0;
  double out = 0.0;
  for (std::size_t n = kRate / 2; n < x.size(); ++n) {
    in += x[n] * x[n];
    out += y[n] * y[n];
  }
  return 10.0 * std::log10(out / in);
}

TEST(LoudnessTest, CountsThePeakAndTheSamplesTheDacClips) {
  const std::vector<int> codes = {0, 130, -140, 5};
  const Loudness l = MeasureLoudness(codes, {}, 3);
  EXPECT_EQ(l.peak, 140);
  EXPECT_DOUBLE_EQ(l.clipped, 0.5);
  EXPECT_EQ(l.wrapped_samples, 3);
}

TEST(LoudnessTest, SoundingRmsLeavesOutSilentFrames) {
  // Frame 0 (silent) is levelled around sample 225, frame 1 (voiced)
  // around 425, each over +-100 samples.
  std::vector<int> codes(800, 0);
  for (std::size_t n = 300; n < codes.size(); ++n) {
    codes[n] = 20;
  }
  const std::vector<lpc::Frame> frames = {
      lpc::Frame::Silence(), lpc::Frame::Voiced(10, 30, kNeutralVoicedK),
      lpc::Frame::Stop()};
  EXPECT_NEAR(MeasureLoudness(codes, frames, 0).sounding_rms, 20.0, 1e-9);
}

TEST(LoudnessTest, HighPassedAndAWeightedLevelsStartAfterTheIdleFrame) {
  const std::vector<int> idle(kLoudnessStartSample, 50);
  const Loudness l = MeasureLoudness(idle, {}, 0);
  EXPECT_EQ(l.high_passed_rms, 0.0);
  EXPECT_EQ(l.a_weighted_rms, 0.0);
}

TEST(LoudnessTest, BbcHighPassRemovesDcAndPassesHighFrequencies) {
  const std::vector<double> dc(4000, 1.0);
  EXPECT_LT(std::fabs(BbcHighPass(dc).back()), 1e-6);
  EXPECT_NEAR(GainDb(BbcHighPass, 3000.0), 0.0, 0.5);
  // A first-order high-pass is 3 dB down at its corner (prewarped).
  EXPECT_NEAR(GainDb(BbcHighPass, kBbcHighPassHz), -3.01, 0.05);
}

TEST(LoudnessTest, AWeightingFollowsTheStandardCurve) {
  // IEC 61672-1, Table 3: A-weighting at 1 kHz, 100 Hz and 2 kHz. The
  // bilinear transform warps the analogue curve little below 2 kHz at
  // 8 kHz.
  EXPECT_NEAR(GainDb(AWeighted, 1000.0), 0.0, 0.2);
  EXPECT_NEAR(GainDb(AWeighted, 100.0), -19.1, 0.3);
  EXPECT_NEAR(GainDb(AWeighted, 2000.0), 1.2, 0.5);
}

}  // namespace
}  // namespace speak5220::core::quality
