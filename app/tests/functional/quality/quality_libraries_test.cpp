/*
 * File:        quality_libraries_test.cpp
 * Module:      quality tests
 * Purpose:     Functional tests of the adapters over the quality measures'
 *              libraries: libfvad's voice activity, SPTK's LPC envelopes
 *              and its REAPER and Harvest pitch estimators
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

#include "core/quality/lpc_envelope.h"
#include "core/quality/pitch_estimator.h"
#include "core/quality/voice_activity.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::quality {
namespace {

constexpr double kRate = 8000.0;

// `seconds` of a 160 Hz pulse train (a whole 50-sample period) through a
// two-pole resonance at 700 Hz, a crude voiced vowel, peaking near 0.5.
std::vector<double> Vowel(double seconds) {
  const auto n = static_cast<std::size_t>(seconds * kRate);
  constexpr double kF0 = 160.0;
  constexpr double kFormant = 700.0;
  constexpr double kRadius = 0.97;
  const double b1 =
      2.0 * kRadius * std::cos(2.0 * std::numbers::pi * kFormant / kRate);
  const double b2 = -kRadius * kRadius;
  std::vector<double> x(n);
  double y1 = 0.0;
  double y2 = 0.0;
  const double period = kRate / kF0;
  double next_pulse = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    double e = 0.0;
    if (static_cast<double>(i) >= next_pulse) {
      e = 1.0;
      next_pulse += period;
    }
    const double y = e + b1 * y1 + b2 * y2;
    y2 = y1;
    y1 = y;
    x[i] = y;
  }
  double peak = 0.0;
  for (const double v : x) {
    peak = std::max(peak, std::abs(v));
  }
  for (double& v : x) {
    v *= 0.5 / peak;
  }
  return x;
}

// Silence, the vowel and silence, `seconds` each.
std::vector<double> SilenceVowelSilence(double seconds) {
  const auto gap = static_cast<std::size_t>(seconds * kRate);
  std::vector<double> x(gap, 0.0);
  const std::vector<double> vowel = Vowel(seconds);
  x.insert(x.end(), vowel.begin(), vowel.end());
  x.insert(x.end(), gap, 0.0);
  return x;
}

TEST(QualityLibrariesTest, FvadMarksTheVowelAndNotTheSilence) {
  const std::vector<double> x = SilenceVowelSilence(0.5);
  FvadVoiceActivity vad(2);
  const StatusOr<std::vector<bool>> speech = vad.SpeechFrames(x);
  ASSERT_TRUE(speech.IsOk()) << speech.GetStatus().ToString();
  ASSERT_EQ(speech->size(), x.size() / kVoiceActivityFrame);
  // 50 frames each: silence, vowel, silence. Allow the detector a few
  // frames of hangover at the end of the vowel.
  int silence_marked = 0;
  int vowel_marked = 0;
  for (std::size_t f = 0; f < speech->size(); ++f) {
    if (f >= 50 && f < 100) {
      vowel_marked += (*speech)[f] ? 1 : 0;
    } else if (f < 50 || f >= 110) {
      silence_marked += (*speech)[f] ? 1 : 0;
    }
  }
  EXPECT_GE(vowel_marked, 45);
  EXPECT_EQ(silence_marked, 0);
}

TEST(QualityLibrariesTest, SptkEnvelopePeaksAtTheFormant) {
  const std::vector<double> vowel = Vowel(0.1);
  // A Hamming-windowed 25 ms frame from the middle.
  constexpr int kFrame = 200;
  std::vector<double> frame(kFrame);
  for (int i = 0; i < kFrame; ++i) {
    const double w =
        0.54 - 0.46 * std::cos(2.0 * std::numbers::pi * i / (kFrame - 1));
    frame[i] = vowel[300 + static_cast<std::size_t>(i)] * w;
  }
  SptkLpcAnalysis lpc;
  const StatusOr<LpcFit> fit = lpc.Analyse(frame, 10);
  ASSERT_TRUE(fit.IsOk()) << fit.GetStatus().ToString();
  EXPECT_EQ(fit->a.size(), 10U);
  EXPECT_GT(fit->gain, 0.0);
  constexpr int kFft = 512;
  const StatusOr<std::vector<double>> db = lpc.EnvelopeDb(*fit, kFft);
  ASSERT_TRUE(db.IsOk()) << db.GetStatus().ToString();
  ASSERT_EQ(db->size(), static_cast<std::size_t>(kFft / 2 + 1));
  std::size_t peak = 0;
  for (std::size_t k = 0; k < db->size(); ++k) {
    if ((*db)[k] > (*db)[peak]) {
      peak = k;
    }
  }
  const double peak_hz = static_cast<double>(peak) * kRate / kFft;
  EXPECT_NEAR(peak_hz, 700.0, 50.0);
}

TEST(QualityLibrariesTest, SptkRefusesASilentFrame) {
  const std::vector<double> silence(200, 0.0);
  EXPECT_FALSE(SptkLpcAnalysis().Analyse(silence, 10).IsOk());
}

// Both estimators find the vowel's 160 Hz in its middle, within 1%, and nothing
// in the silence around it.
void ExpectTracksTheVowel(PitchAlgorithm algorithm) {
  const std::vector<double> x = SilenceVowelSilence(0.5);
  SptkPitchEstimator estimator(algorithm);
  const StatusOr<std::vector<double>> f0 = estimator.Track(x);
  ASSERT_TRUE(f0.IsOk()) << f0.GetStatus().ToString();
  ASSERT_EQ(f0->size(), (x.size() + kPitchHop - 1) / kPitchHop);
  for (std::size_t f = 60; f < 90; ++f) {
    EXPECT_NEAR((*f0)[f], 160.0, 1.6)
        << PitchAlgorithmName(algorithm) << " frame " << f;
  }
  for (std::size_t f = 0; f < 40; ++f) {
    EXPECT_EQ((*f0)[f], 0.0) << PitchAlgorithmName(algorithm) << " frame " << f;
  }
}

// Plan Phase 8, task 9: on pulse trains at 8 kHz both estimators find F0
// within 1 %, at each F0 with a whole period in samples.
void ExpectFindsPulseTrainF0(PitchAlgorithm algorithm) {
  SptkPitchEstimator estimator(algorithm);
  for (const int period : {80, 64, 50, 40, 32}) {
    const double f0 = kRate / period;
    std::vector<double> x(static_cast<std::size_t>(kRate), 0.0);
    for (std::size_t i = 0; i < x.size();
         i += static_cast<std::size_t>(period)) {
      x[i] = 0.5;
    }
    const StatusOr<std::vector<double>> track = estimator.Track(x);
    ASSERT_TRUE(track.IsOk()) << track.GetStatus().ToString();
    for (std::size_t f = 20; f < 80; ++f) {
      EXPECT_NEAR((*track)[f], f0, 0.01 * f0)
          << PitchAlgorithmName(algorithm) << " at " << f0 << " Hz, frame "
          << f;
    }
  }
}

TEST(QualityLibrariesTest, ReaperFindsPulseTrainF0WithinOnePercent) {
  ExpectFindsPulseTrainF0(PitchAlgorithm::kReaper);
}

TEST(QualityLibrariesTest, HarvestFindsPulseTrainF0WithinOnePercent) {
  ExpectFindsPulseTrainF0(PitchAlgorithm::kHarvest);
}

TEST(QualityLibrariesTest, ReaperTracksTheVowel) {
  ExpectTracksTheVowel(PitchAlgorithm::kReaper);
}

TEST(QualityLibrariesTest, HarvestTracksTheVowel) {
  ExpectTracksTheVowel(PitchAlgorithm::kHarvest);
}

}  // namespace
}  // namespace speak5220::core::quality
