/*
 * File:        anchor_vocoder_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the anchor vocoder: its pitch tracker, LPC
 *              analysis and synthesis
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/anchor_vocoder.h"

#include <cmath>
#include <cstddef>
#include <vector>

#include "core/quality/numpy_normal.h"
#include "gtest/gtest.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

// The 10 ms analysis frame around a sample.
std::size_t AnalysisFrame(int sample) {
  return static_cast<std::size_t>(sample / 80);
}

TEST(AnchorVocoderTest, PitchTrackerFindsTheFundamentalOfAVowel) {
  const std::vector<double> x = testing::SyntheticSpeech();
  const AnchorPitch pitch = TrackAnchorPitch(x);
  ASSERT_EQ(pitch.f0.size(), x.size() / 80 + 1);
  const std::size_t mid = AnalysisFrame(
      (testing::kSecondVowelStart + testing::kSecondVowelEnd) / 2);
  EXPECT_TRUE(pitch.voiced[mid]);
  EXPECT_NEAR(pitch.f0[mid], testing::kSecondVowelHz, 4.0);
}

TEST(AnchorVocoderTest, AnalysisGivesVoicedFramesAPeriodAndStableFilters) {
  const std::vector<double> x = testing::SyntheticSpeech();
  const std::vector<AnchorFrame> frames = AnalyseAnchor(x);
  ASSERT_EQ(frames.size(), x.size() / 200 + 1);
  const AnchorFrame& voiced = frames[static_cast<std::size_t>(
      (testing::kSecondVowelStart + testing::kSecondVowelEnd) / 2 / 200)];
  EXPECT_NEAR(voiced.period, 8000.0 / testing::kSecondVowelHz, 1.0);
  EXPECT_GT(voiced.gain, 0.0);
  EXPECT_EQ(frames.front().period, 0.0);  // the opening pause
  for (const AnchorFrame& f : frames) {
    for (double k : f.k) {
      EXPECT_LT(std::fabs(k), 1.0);
    }
  }
}

TEST(AnchorVocoderTest, UnvoicedFramesDrawOneNoiseValuePerSample) {
  AnchorFrame unvoiced;
  unvoiced.gain = 1.0;
  const std::vector<AnchorFrame> frames(3, unvoiced);
  NumpyNormal noise = NumpyNormal::SeedOne();
  const std::vector<double> y = SynthesiseAnchor(frames, 500, noise);
  ASSERT_EQ(y.size(), 500U);
  NumpyNormal reference = NumpyNormal::SeedOne();
  for (int i = 0; i < 3 * 200; ++i) {
    reference.Next();
  }
  EXPECT_EQ(noise.Next(), reference.Next());
}

TEST(AnchorVocoderTest, VoicedFramesUseNoNoise) {
  AnchorFrame voiced;
  voiced.gain = 1.0;
  voiced.period = 80.0;
  const std::vector<AnchorFrame> frames(3, voiced);
  NumpyNormal noise = NumpyNormal::SeedOne();
  const std::vector<double> y = SynthesiseAnchor(frames, 600, noise);
  EXPECT_EQ(noise.Next(), NumpyNormal::SeedOne().Next());
  // A pulse every 80 samples through an all-pass (k = 0) filter, then the
  // de-emphasis: once the gain has settled the output is periodic, to the
  // de-emphasis's decay over a period (0.9375^80).
  EXPECT_NEAR(y[380], y[460], 0.01 * std::fabs(y[380]));
}

TEST(AnchorVocoderTest, VocoderIsDeterministicAndKeepsTheLength) {
  const std::vector<double> x = testing::SyntheticSpeech();
  const std::vector<double> a = AnchorVocoder(x);
  EXPECT_EQ(a.size(), x.size());
  EXPECT_EQ(a, AnchorVocoder(x));
}

TEST(AnchorVocoderTest, AnchorScoreIsAPlausibleCeiling) {
  const auto score = AnchorScore(testing::SyntheticSpeech());
  ASSERT_TRUE(score.IsOk()) << score.GetStatus().ToString();
  EXPECT_GT(*score, 1.5);
  EXPECT_LT(*score, 4.6);
}

}  // namespace
}  // namespace speak5220::core::quality
