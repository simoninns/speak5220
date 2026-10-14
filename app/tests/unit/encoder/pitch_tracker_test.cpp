/*
 * File:        pitch_tracker_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the YIN pitch tracker on synthetic pulse
 *              trains, noise and silence, with and without an anchor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/pitch_tracker.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <numbers>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder_options.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/encoder_mocks.h"
#include "support/quality_mocks.h"

namespace speak5220::core::encoder {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
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

// Pulses through a two-pole resonator at `formant` Hz of pole radius
// `radius`, a pulse wherever the phase, advancing by 1 / period(n) a
// sample, passes a whole number.
std::vector<double> Voice(int length, const std::function<double(int)>& period,
                          double formant, double radius) {
  std::vector<double> x(static_cast<std::size_t>(length), 0.0);
  const double theta = 2.0 * std::numbers::pi * formant / 8000.0;
  double phase = 0.0;
  double y1 = 0.0;
  double y2 = 0.0;
  for (int n = 0; n < length; ++n) {
    const double next = phase + (1.0 / period(n));
    const double pulse = std::floor(next) > std::floor(phase) ? 1.0 : 0.0;
    phase = next;
    const double y =
        pulse + (2.0 * radius * std::cos(theta) * y1) - (radius * radius * y2);
    x[static_cast<std::size_t>(n)] = 0.05 * y;
    y2 = y1;
    y1 = y;
  }
  return x;
}

// An anchor's track of a signal of `length` samples: 8000 / period(80k) Hz
// at each 10 ms value k, from sample `from` to `to`; 0 elsewhere.
std::vector<double> AnchorF0(int length,
                             const std::function<double(int)>& period, int from,
                             int to) {
  std::vector<double> f0;
  for (int at = 0; at < length; at += 80) {
    f0.push_back(at >= from && at <= to ? 8000.0 / period(at) : 0.0);
  }
  return f0;
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

TEST(PitchTrackerTest, TracksPulseTrainsFrom16To160Samples) {
  dsp::PocketFft fft;
  YinPitchTracker tracker(fft);
  for (const int period : {16, 24, 40, 64, 100, 130, 160}) {
    const PitchTrack track = tracker.Track(PulseTrain(period, kSeconds1), {});
    for (std::size_t h = 10; h + 10 < track.periods.size(); ++h) {
      EXPECT_NEAR(track.periods[h], period, 0.02 * period)
          << "period " << period << ", hop " << h;
    }
  }
}

TEST(PitchTrackerTest, AnchorKeepsTheTrackOffABoostedFourthHarmonic) {
  // The `six` case: a 117.6 Hz voice whose narrow first formant sits on
  // its 4th harmonic, its period wandering by up to 3 % from cycle to
  // cycle, so the 1 kHz low-passed signal repeats more strongly every 17
  // samples than every 68.
  std::vector<double> cycles;
  std::uint32_t state = 777;
  for (int c = 0; c <= kSeconds1 / 60; ++c) {
    state = (state * 1664525U) + 1013904223U;
    const double u = (static_cast<double>(state >> 8) / 16777216.0) - 0.5;
    cycles.push_back(68.0 * (1.0 + (0.06 * u)));
  }
  const auto period = [&](int n) {
    return cycles[static_cast<std::size_t>(n / 68)];
  };
  const std::vector<double> x = Voice(kSeconds1, period, 470.6, 0.985);
  dsp::PocketFft fft;
  YinPitchTracker alone(fft);
  const PitchTrack locked = alone.Track(x, {});
  testing::MockPitchEstimator anchor;
  EXPECT_CALL(anchor, Track(SizeIs(kSeconds1)))
      .WillOnce(Return(AnchorF0(kSeconds1, period, 0, kSeconds1)));
  YinPitchTracker anchored(fft, anchor);
  const PitchTrack track = anchored.Track(x, {});
  int locked_hops = 0;
  for (std::size_t h = 10; h + 10 < track.periods.size(); ++h) {
    locked_hops += std::abs(locked.periods[h] - 17.0) < 1.5 ? 1 : 0;
    EXPECT_NEAR(track.periods[h], 68.0, 0.06 * 68.0) << "hop " << h;
  }
  // Without the anchor, nearly every hop is on the harmonic.
  EXPECT_GT(locked_hops, static_cast<int>(track.periods.size() - 20) * 9 / 10);
}

TEST(PitchTrackerTest, FollowsAnOctaveGlideOver100Ms) {
  // 100 Hz to 200 Hz over 800 samples, from sample 3600.
  const auto period = [](int n) {
    const double t = std::clamp((n - 3600) / 800.0, 0.0, 1.0);
    return 80.0 * std::pow(2.0, -t);
  };
  const std::vector<double> x = Voice(kSeconds1, period, 500.0, 0.95);
  dsp::PocketFft fft;
  testing::MockPitchEstimator anchor;
  EXPECT_CALL(anchor, Track(_))
      .WillOnce(Return(AnchorF0(kSeconds1, period, 0, kSeconds1)));
  YinPitchTracker tracker(fft, anchor);
  const PitchTrack track = tracker.Track(x, {});
  // Within 6 %: YIN's 30 ms window lags the glide a little at its start.
  for (std::size_t h = 10; h + 10 < track.periods.size(); ++h) {
    const double expected = period(track.hops[h]);
    EXPECT_NEAR(track.periods[h], expected, 0.06 * expected) << "hop " << h;
  }
}

TEST(PitchTrackerTest, AnchorThatFailsOrFindsNoVoicingChangesNothing) {
  const std::vector<double> x = PulseTrain(70, kSeconds1);
  dsp::PocketFft fft;
  YinPitchTracker alone(fft);
  const PitchTrack expected = alone.Track(x, {});
  testing::MockPitchEstimator failing;
  EXPECT_CALL(failing, Track(_))
      .WillOnce(Return(FailedPreconditionError("cannot track")));
  YinPitchTracker with_failing(fft, failing);
  EXPECT_EQ(with_failing.Track(x, {}).periods, expected.periods);
  testing::MockPitchEstimator silent;
  EXPECT_CALL(silent, Track(_))
      .WillOnce(Return(std::vector<double>(kSeconds1 / 80, 0.0)));
  YinPitchTracker with_silent(fft, silent);
  EXPECT_EQ(with_silent.Track(x, {}).periods, expected.periods);
}

TEST(PitchTrackerTest, AnchorDoesNotVoiceNoise) {
  dsp::PocketFft fft;
  testing::MockPitchEstimator anchor;
  EXPECT_CALL(anchor, Track(_))
      .WillOnce(
          Return(AnchorF0(kSeconds1, [](int) { return 80.0; }, 0, kSeconds1)));
  YinPitchTracker tracker(fft, anchor);
  const PitchTrack track = tracker.Track(Noise(kSeconds1), {});
  int voiced = 0;
  for (const double p : track.periods) {
    voiced += p > 0.0 ? 1 : 0;
  }
  EXPECT_LT(voiced, static_cast<int>(track.periods.size()) / 10);
}

TEST(PitchTrackerTest, AnchorPeriodsFillOddHopsFromTheValuesEitherSide) {
  // Values every 10 ms: 100 Hz, 0, 200 Hz, 400 Hz.
  const std::vector<double> f0 = {100.0, 0.0, 200.0, 400.0};
  // Hop 1 has one voiced neighbour, hop 3 the other, hop 5 two (the
  // geometric mean, 282.8 Hz) and hop 7 none past the end.
  EXPECT_THAT(
      AnchorPeriods(f0, 8),
      ElementsAre(80.0, 80.0, 0.0, 40.0, 40.0,
                  ::testing::DoubleNear(8000.0 / std::sqrt(80000.0), 1e-12),
                  20.0, 20.0));
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
