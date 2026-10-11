/*
 * File:        pesq_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of ScorePesq() on synthetic signals and of the
 *              P.862.1 mapping
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pesq.h"

#include <cmath>
#include <limits>
#include <vector>

#include "gtest/gtest.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

std::vector<double> Plus(const std::vector<double>& a,
                         const std::vector<double>& b) {
  std::vector<double> y(a.size());
  for (std::size_t i = 0; i < a.size(); ++i) {
    y[i] = a[i] + b[i];
  }
  return y;
}

TEST(PesqTest, MosLqoMapsTheRawScale) {
  // ITU-T P.862.1: 0.999 + 4 / (1 + exp(-1.4945 x + 4.6607)).
  EXPECT_NEAR(MosLqoFromRaw(4.5F), 4.549, 1e-3);
  EXPECT_NEAR(MosLqoFromRaw(1.0F), 1.160, 1e-3);
  EXPECT_LT(MosLqoFromRaw(-0.5F), 1.03F);
  EXPECT_LT(MosLqoFromRaw(2.0F), MosLqoFromRaw(2.1F));
}

TEST(PesqTest, IdenticalSignalsScoreAtTheTop) {
  const std::vector<double> x = testing::SyntheticSpeech();
  const auto score = ScorePesq(x, x);
  ASSERT_TRUE(score.IsOk()) << score.GetStatus().ToString();
  EXPECT_NEAR(score->raw, 4.5F, 0.05F);
  EXPECT_GT(score->mos_lqo, 4.4F);
}

TEST(PesqTest, ScoreIgnoresScaleAndDelay) {
  const std::vector<double> x = testing::SyntheticSpeech();
  std::vector<double> scaled = testing::Delayed(x, 160);
  for (double& v : scaled) {
    v *= 0.05;
  }
  const auto score = ScorePesq(x, scaled);
  ASSERT_TRUE(score.IsOk()) << score.GetStatus().ToString();
  EXPECT_GT(score->mos_lqo, 4.3F);
}

TEST(PesqTest, NoiseLowersTheScore) {
  const std::vector<double> x = testing::SyntheticSpeech();
  const std::vector<double> light =
      Plus(x, testing::WhiteNoise(static_cast<int>(x.size()), 0.003));
  const std::vector<double> heavy =
      Plus(x, testing::WhiteNoise(static_cast<int>(x.size()), 0.05));
  const auto a = ScorePesq(x, light);
  const auto b = ScorePesq(x, heavy);
  ASSERT_TRUE(a.IsOk() && b.IsOk());
  EXPECT_LT(a->mos_lqo, 4.4F);
  EXPECT_LT(b->mos_lqo, a->mos_lqo);
  EXPECT_LT(b->mos_lqo, 2.5F);
}

TEST(PesqTest, RejectsShortSilentAndNonFiniteInput) {
  const std::vector<double> x = testing::SyntheticSpeech();
  const std::vector<double> short_input(kPesqMinSamples - 1, 0.1);
  EXPECT_FALSE(ScorePesq(short_input, x).IsOk());
  EXPECT_FALSE(ScorePesq(x, short_input).IsOk());
  const std::vector<double> silent(x.size(), 0.0);
  EXPECT_FALSE(ScorePesq(silent, silent).IsOk());
  std::vector<double> bad = x;
  bad[5000] = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(ScorePesq(x, bad).IsOk());
}

}  // namespace
}  // namespace speak5220::core::quality
