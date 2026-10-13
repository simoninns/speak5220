/*
 * File:        normaliser_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of the normaliser: the speech-gated loudness, the
 *              level it brings recordings to, the compressor and the peak
 *              limiter
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/normaliser.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

constexpr int kRate = 8000;
// Half a second at 8 kHz.
constexpr std::size_t kStretch = 4000;

// A 200 Hz sine whose amplitude is `amplitudes[k]` over the k-th half
// second.
std::vector<double> Stretches(const std::vector<double>& amplitudes) {
  std::vector<double> x(amplitudes.size() * kStretch);
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] = amplitudes[i / kStretch] * std::sin(2.0 * std::numbers::pi * 200.0 *
                                               static_cast<double>(i) / kRate);
  }
  return x;
}

// The RMS, in dB, of x[begin, end).
double RmsDb(const std::vector<double>& x, std::size_t begin, std::size_t end) {
  double sum = 0.0;
  for (std::size_t i = begin; i < end; ++i) {
    sum += x[i] * x[i];
  }
  return 10.0 * std::log10(sum / static_cast<double>(end - begin));
}

TEST(SpeechLoudnessTest, IsTheMeanPowerOfASteadySound) {
  const std::optional<double> loudness =
      SpeechLoudness(Stretches({0.5, 0.5}), kRate);
  ASSERT_TRUE(loudness.has_value());
  EXPECT_NEAR(*loudness, 10.0 * std::log10(0.125), 0.01);
}

// Pauses and quiet stretches, such as breaths, do not pull it down (the
// 3 dB of averaging them in), but for the blocks partly over them.
TEST(SpeechLoudnessTest, LeavesOutSilenceAndQuietStretches) {
  const double steady = *SpeechLoudness(Stretches({0.5, 0.5}), kRate);
  EXPECT_NEAR(*SpeechLoudness(Stretches({0.0, 0.5, 0.0, 0.5}), kRate), steady,
              0.6);
  EXPECT_NEAR(*SpeechLoudness(Stretches({0.02, 0.5, 0.02, 0.5}), kRate), steady,
              0.6);
  EXPECT_FALSE(SpeechLoudness(Stretches({0.0, 0.0}), kRate).has_value());
  EXPECT_FALSE(SpeechLoudness(std::vector<double>{}, kRate).has_value());
}

TEST(SpeechLoudnessTest, OneShortBlockForAShortRecording) {
  const std::vector<double> x(100, 0.25);
  EXPECT_NEAR(*SpeechLoudness(x, kRate), 20.0 * std::log10(0.25), 1e-9);
}

// Without compression, a recording is brought to the target loudness.
TEST(NormaliserTest, BringsTheSpeechToTheTarget) {
  const std::vector<double> y =
      Normalise(Stretches({0.0, 0.01, 0.0}), kRate, -20.0, 1.0);
  EXPECT_NEAR(*SpeechLoudness(y, kRate), -20.0, 1e-9);
}

// Recordings of the same sound at different levels come out the same.
TEST(NormaliserTest, LevelOfTheRecordingDoesNotMatter) {
  const std::vector<double> loud = Stretches({0.8, 0.2, 0.5});
  const std::vector<double> quiet = Stretches({0.008, 0.002, 0.005});
  const std::vector<double> a = Normalise(loud, kRate, -18.0, 4.0);
  const std::vector<double> b = Normalise(quiet, kRate, -18.0, 4.0);
  ASSERT_EQ(a.size(), b.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    ASSERT_NEAR(a[i], b[i], 1e-9) << i;
  }
}

// The compressor narrows the gap between loud and soft stretches, and the
// make-up gain keeps the loudness at the target.
TEST(NormaliserTest, CompressionEvensOutLoudAndSoft) {
  // 12 dB apart.
  const std::vector<double> x = Stretches({0.5, 0.125, 0.5, 0.125});
  const double apart = 20.0 * std::log10(4.0);
  const auto gap = [](const std::vector<double>& y) {
    return RmsDb(y, 1000, kStretch - 1000) -
           RmsDb(y, kStretch + 1000, (2 * kStretch) - 1000);
  };
  EXPECT_NEAR(gap(Normalise(x, kRate, -24.0, 1.0)), apart, 1e-6);
  const std::vector<double> compressed = Normalise(x, kRate, -24.0, 4.0);
  EXPECT_LT(gap(compressed), 8.0);
  EXPECT_GT(gap(compressed), 0.0);
  EXPECT_NEAR(*SpeechLoudness(compressed, kRate), -24.0, 1e-6);
}

// No sample comes out over the ceiling, 1 dB below full scale, however loud
// the target, and the limiter turns down only around the peaks.
TEST(NormaliserTest, LimitsThePeaksToTheCeiling) {
  std::vector<double> x = Stretches({0.1, 0.1});
  x[kStretch] = 0.9;
  const std::vector<double> y = Normalise(x, kRate, -6.0, 1.0);
  const double ceiling = std::pow(10.0, -1.0 / 20.0);
  for (std::size_t i = 0; i < y.size(); ++i) {
    ASSERT_LE(std::abs(y[i]), ceiling + 1e-12) << i;
  }
  // A 200 Hz sine at -6 dB has peaks at -3 dB: untouched away from the
  // spike.
  const double free = std::pow(10.0, -6.0 / 20.0) * std::numbers::sqrt2;
  const auto peak = [&y](std::size_t begin, std::size_t end) {
    double most = 0.0;
    for (std::size_t i = begin; i < end; ++i) {
      most = std::max(most, std::abs(y[i]));
    }
    return most;
  };
  EXPECT_NEAR(peak(0, 1000), free, 0.01);
  EXPECT_LT(peak(kStretch - 40, kStretch + 40), ceiling + 1e-12);
}

TEST(NormaliserTest, SilenceAndNothingComeBackUnchanged) {
  const std::vector<double> silence(1000, 0.0);
  EXPECT_EQ(Normalise(silence, kRate, -20.0, 3.0), silence);
  EXPECT_TRUE(Normalise(std::vector<double>{}, kRate, -20.0, 3.0).empty());
}

}  // namespace
}  // namespace speak5220::core::dsp
