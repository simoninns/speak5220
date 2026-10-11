/*
 * File:        output_alignment_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of lining the chip's output up with its input
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/output_alignment.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

TEST(OutputAlignmentTest, FramesAreCentredOnEachHop) {
  EXPECT_EQ(AnalysisFrameCount(800), 11);
  std::vector<double> x(400);
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] = static_cast<double>(i);
  }
  const std::vector<double> f = CentredFrame(x, 2, 8, 80);
  ASSERT_EQ(f.size(), 8U);
  EXPECT_EQ(f[0], 156.0);
  EXPECT_EQ(f[7], 163.0);
  // Zero-padded before the start.
  EXPECT_EQ(CentredFrame(x, 0, 8, 80)[3], 0.0);
  EXPECT_EQ(CentredFrame(x, 0, 8, 80)[5], 1.0);
}

TEST(OutputAlignmentTest, FrameLevelsGiveASineItsPower) {
  std::vector<double> x(8000);
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] = std::sin(2 * std::numbers::pi * 1000.0 * static_cast<double>(i) /
                    8000.0);
  }
  const std::vector<double> levels = FrameLevelsDb(x);
  // A unit sine's mean square is 1/2: -3 dB.
  EXPECT_NEAR(levels[50], 10 * std::log10(0.5), 0.05);
}

TEST(OutputAlignmentTest, MelBankTrianglesPeakAtOne) {
  const std::vector<double> fb = MelFilterBank(20);
  ASSERT_EQ(fb.size(), 20U * 129U);
  for (int band = 0; band < 20; ++band) {
    double peak = 0.0;
    for (int bin = 0; bin < 129; ++bin) {
      const double w = fb[(static_cast<std::size_t>(band) * 129) +
                          static_cast<std::size_t>(bin)];
      EXPECT_GE(w, 0.0);
      peak = std::max(peak, w);
    }
    EXPECT_GT(peak, 0.5) << band;
    EXPECT_LE(peak, 1.0) << band;
  }
}

TEST(OutputAlignmentTest, PlaceDelaysAndCuts) {
  const std::vector<double> out = {1, 2, 3, 4};
  EXPECT_EQ(Place(out, 2, 5), (std::vector<double>{0, 0, 1, 2, 3}));
  EXPECT_EQ(Place(out, -1, 5), (std::vector<double>{2, 3, 4, 0, 0}));
}

TEST(OutputAlignmentTest, MatchLevelEqualisesSpeechPower) {
  const std::vector<double> x = testing::SyntheticSpeech();
  std::vector<double> quiet = x;
  for (double& v : quiet) {
    v *= 0.1;
  }
  const std::vector<double> matched = MatchLevel(x, quiet);
  for (std::size_t i = 0; i < x.size(); i += 997) {
    EXPECT_NEAR(matched[i], x[i], 1e-9);
  }
  const std::vector<double> silent(x.size(), 0.0);
  EXPECT_EQ(MatchLevel(x, silent), silent);
}

TEST(OutputAlignmentTest, LevelMatchGainIsTheRatioOfSpeechLevels) {
  const std::vector<double> x = testing::SyntheticSpeech();
  std::vector<double> quiet = x;
  for (double& v : quiet) {
    v *= 0.1;
  }
  EXPECT_NEAR(LevelMatchGain(x, quiet), 10.0, 1e-9);
  EXPECT_EQ(LevelMatchGain(x, std::vector<double>(x.size(), 0.0)), 1.0);
}

TEST(OutputAlignmentTest, AlignmentFindsTheOutputsDelay) {
  constexpr int kDelay = 237;
  const std::vector<double> x = testing::SyntheticSpeech();
  // The output starts at its sound, as the chip's is cut.
  std::vector<double> out(x.begin() + 1000, x.end());
  for (double& v : out) {
    v *= 30.0;
  }
  const int lag = AlignByEnvelope(x, out);
  EXPECT_EQ(lag % 20, 0);
  EXPECT_LE(std::abs(lag - 1000), 40);
  const AlignedOutput aligned = AlignOutput(x, testing::Delayed(out, kDelay));
  EXPECT_LE(std::abs(aligned.lag - (1000 - kDelay)), 5);
  ASSERT_EQ(aligned.degraded.size(), x.size());
}

}  // namespace
}  // namespace speak5220::core::quality
