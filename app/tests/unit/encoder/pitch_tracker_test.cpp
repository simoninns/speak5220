/*
 * File:        pitch_tracker_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the YIN pitch tracker on synthetic pulse
 *              trains, noise and silence
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/pitch_tracker.h"

#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder_options.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/encoder_mocks.h"

namespace speak5220::core::encoder {
namespace {

using ::testing::_;
using ::testing::Return;
using ::testing::SizeIs;

constexpr int kSeconds1 = 8000;

// A decaying-resonance pulse train: a pulse every `period` samples through
// a two-pole resonator at 500 Hz, like a vowel.
std::vector<double> PulseTrain(int period, int length) {
  std::vector<double> x(static_cast<std::size_t>(length), 0.0);
  const double radius = 0.95;
  const double theta = 2.0 * std::numbers::pi * 500.0 / 8000.0;
  double y1 = 0.0;
  double y2 = 0.0;
  for (int n = 0; n < length; ++n) {
    const double pulse = (n % period == 0) ? 1.0 : 0.0;
    const double y =
        pulse + (2.0 * radius * std::cos(theta) * y1) - (radius * radius * y2);
    x[static_cast<std::size_t>(n)] = 0.05 * y;
    y2 = y1;
    y1 = y;
  }
  return x;
}

// Deterministic white noise from a linear congruential generator.
std::vector<double> Noise(int length) {
  std::vector<double> x(static_cast<std::size_t>(length));
  std::uint32_t state = 12345;
  for (double& v : x) {
    state = (state * 1664525U) + 1013904223U;
    v = (static_cast<double>(state >> 8) / 16777216.0 - 0.5) * 0.2;
  }
  return x;
}

TEST(PitchTrackerTest, TracksPeriodOfPulseTrain) {
  dsp::PocketFft fft;
  YinPitchTracker tracker(fft);
  const PitchTrack track = tracker.Track(PulseTrain(80, kSeconds1), {});
  ASSERT_EQ(track.periods.size(), track.hops.size());
  // Away from the edges every hop is voiced at a period of 80 samples.
  for (std::size_t h = 10; h + 10 < track.periods.size(); ++h) {
    EXPECT_NEAR(track.periods[h], 80.0, 1.0) << "hop " << h;
  }
}

TEST(PitchTrackerTest, FindsNoPitchInNoise) {
  dsp::PocketFft fft;
  YinPitchTracker tracker(fft);
  const PitchTrack track = tracker.Track(Noise(kSeconds1), {});
  int voiced = 0;
  for (const double p : track.periods) {
    voiced += p > 0.0 ? 1 : 0;
  }
  EXPECT_LT(voiced, static_cast<int>(track.periods.size()) / 10);
}

TEST(PitchTrackerTest, SilenceIsUnvoiced) {
  dsp::PocketFft fft;
  YinPitchTracker tracker(fft);
  const PitchTrack track = tracker.Track(std::vector<double>(2000, 0.0), {});
  for (const double p : track.periods) {
    EXPECT_EQ(p, 0.0);
  }
}

TEST(PitchTrackerTest, HopsEveryFiveMillisecondsFromZero) {
  dsp::PocketFft fft;
  const YinAnalysis yin = Yin(fft, std::vector<double>(100, 0.0));
  EXPECT_EQ(yin.centres, (std::vector<int>{0, 40, 80}));
  ASSERT_THAT(yin.cmndf, SizeIs(3));
  EXPECT_THAT(yin.cmndf[0], SizeIs(kYinMaxLag + 1));
}

TEST(PitchTrackerTest, YinCorrelatesByTwoForwardAndOneInverseFftPerHop) {
  testing::MockFft fft;
  const std::vector<std::complex<double>> bins(513);
  EXPECT_CALL(fft, Forward(SizeIs(1024))).Times(4).WillRepeatedly(Return(bins));
  EXPECT_CALL(fft, Inverse(_, 1024))
      .Times(2)
      .WillRepeatedly(Return(std::vector<double>(1024, 0.0)));
  const YinAnalysis yin = Yin(fft, std::vector<double>(80, 0.0));
  EXPECT_EQ(yin.centres.size(), 2U);
}

TEST(PitchTrackerTest, CandidatesAreRefinedLocalMinimaLowestFirst) {
  std::vector<double> row(kYinMaxLag + 1, 1.0);
  // A symmetric dip at 40 and a lopsided, lower one at 100.
  row[39] = 0.6;
  row[40] = 0.4;
  row[41] = 0.6;
  row[99] = 0.5;
  row[100] = 0.1;
  row[101] = 0.3;
  const std::vector<PitchCandidate> c = PitchCandidates(row);
  ASSERT_EQ(c.size(), 2U);
  // Parabola through (99, 0.5), (100, 0.1), (101, 0.3): offset 0.5 x
  // (0.5 - 0.3) / 0.6 = 1/6.
  EXPECT_NEAR(c[0].lag, 100.0 + (1.0 / 6.0), 1e-12);
  EXPECT_NEAR(c[0].value, 0.1 - (0.25 * 0.2 / 6.0), 1e-12);
  EXPECT_EQ(c[1].lag, 40.0);
  EXPECT_DOUBLE_EQ(c[1].value, 0.4);
}

TEST(PitchTrackerTest, KeepsAtMostFiveCandidates) {
  std::vector<double> row(kYinMaxLag + 1, 1.0);
  for (int t = 20; t <= 140; t += 15) {
    row[static_cast<std::size_t>(t)] = 0.5;
  }
  EXPECT_EQ(PitchCandidates(row).size(), 5U);
}

TEST(PitchTrackerTest, EvidenceScoresFlatAndLowPassSounds) {
  std::vector<double> alternating(1000);
  std::vector<double> slow(1000);
  for (std::size_t i = 0; i < alternating.size(); ++i) {
    alternating[i] = (i % 2 == 0) ? 0.1 : -0.1;
    slow[i] = 0.1 * std::sin(2.0 * std::numbers::pi * 100.0 *
                             static_cast<double>(i) / 8000.0);
  }
  const std::vector<int> centre = {500};
  const VoicingEvidence fast = MeasureVoicingEvidence(alternating, centre);
  EXPECT_EQ(fast.tilt[0], -2.0);
  // 239 crossings in the 240-sample window, per 10 ms.
  EXPECT_NEAR(fast.crossings[0], 239.0 * 80.0 / 240.0, 1e-12);
  const VoicingEvidence low = MeasureVoicingEvidence(slow, centre);
  EXPECT_EQ(low.tilt[0], 4.0);
  EXPECT_LT(low.crossings[0], 10.0);
}

}  // namespace
}  // namespace speak5220::core::encoder
