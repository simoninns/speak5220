/*
 * File:        squelch_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of the squelch: what it mutes and keeps, its hold,
 *              lead and soft edges, and the stretches it reports
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/squelch.h"

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

// Loud (1.0), then quiet, then loud again, each half a second. The
// samples are constant, so the output over the input is the gain.
std::vector<double> LoudQuietLoud() {
  std::vector<double> x(3 * kStretch, 1.0);
  for (std::size_t i = kStretch; i < 2 * kStretch; ++i) {
    x[i] = kQuiet;
  }
  return x;
}

TEST(SquelchTest, MutesStretchesBelowTheThreshold) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> y = Squelch(x, kRate, 30.0);
  ASSERT_EQ(y.size(), x.size());
  // The middle of the quiet stretch is muted; the loud sound is untouched.
  EXPECT_EQ(y[kStretch + (kStretch / 2)], 0.0);
  EXPECT_EQ(y[kStretch / 2], 1.0);
  EXPECT_EQ(y[(2 * kStretch) + (kStretch / 2)], 1.0);
}

TEST(SquelchTest, KeepsStretchesAboveTheThreshold) {
  const std::vector<double> x = LoudQuietLoud();
  EXPECT_EQ(Squelch(x, kRate, 70.0), x);
}

TEST(SquelchTest, HoldsAfterTheSoundAndOpensJustBeforeIt) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> y = Squelch(x, kRate, 30.0);
  // 40 ms after the sound falls: within the 60 ms hold.
  EXPECT_EQ(y[kStretch + 320], kQuiet);
  // 100 ms after: past the hold and its ramp.
  EXPECT_EQ(y[kStretch + 800], 0.0);
  // 5 ms before the sound rises again: within the lead.
  EXPECT_EQ(y[(2 * kStretch) - 40], kQuiet);
  // 30 ms before: past the lead and its ramp.
  EXPECT_EQ(y[(2 * kStretch) - 240], 0.0);
}

TEST(SquelchTest, GainMovesSoftlyAtTheEdges) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> y = Squelch(x, kRate, 30.0);
  // The 5 ms ramp is 40 samples.
  const double max_step = (1.0 / 40.0) + 1e-12;
  for (std::size_t i = kStretch + 1; i < 2 * kStretch; ++i) {
    const double gain = y[i] / x[i];
    const double before = y[i - 1] / x[i - 1];
    EXPECT_LE(std::abs(gain - before), max_step) << "at sample " << i;
  }
}

TEST(SquelchTest, SpansAreWhereTheGainIsLowered) {
  const std::vector<double> x = LoudQuietLoud();
  const std::vector<double> gain = SquelchGain(x, kRate, 30.0);
  const std::vector<SampleSpan> spans = SquelchedSpans(x, kRate, 30.0);
  // One stretch, inside the quiet one: the hold and the lead keep its
  // edges.
  ASSERT_EQ(spans.size(), 1U);
  EXPECT_GT(spans[0].begin, kStretch);
  EXPECT_LT(spans[0].end, 2 * kStretch);
  for (std::size_t i = 0; i < x.size(); ++i) {
    const bool inside = i >= spans[0].begin && i < spans[0].end;
    EXPECT_EQ(gain[i] < 1.0, inside) << "at sample " << i;
  }
  EXPECT_TRUE(SquelchedSpans(x, kRate, 70.0).empty());
}

TEST(SquelchTest, SilenceAndNothingComeBackUnchanged) {
  const std::vector<double> silence(1000, 0.0);
  EXPECT_EQ(Squelch(silence, kRate, 30.0), silence);
  EXPECT_TRUE(Squelch(std::vector<double>{}, kRate, 30.0).empty());
  EXPECT_TRUE(SquelchedSpans(silence, kRate, 30.0).empty());
}

}  // namespace
}  // namespace speak5220::core::dsp
