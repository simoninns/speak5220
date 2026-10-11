/*
 * File:        lpc_analysis_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of autocorrelation, Levinson-Durbin, step-up,
 *              prediction error and log-area ratios
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/lpc_analysis.h"

#include <cmath>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

constexpr double kTolerance = 1e-12;

TEST(LpcAnalysisTest, AutocorrelationSumsLaggedProducts) {
  const std::vector<double> s = {1.0, 2.0, 3.0};
  EXPECT_EQ(Autocorrelation(s, 3), (std::vector<double>{14.0, 8.0, 3.0, 0.0}));
}

// An AR(1) process x[n] = a x[n-1] + e[n] has r[i] = a^i: one reflection
// coefficient, -a in the convention e(n) = s(n) + sum a_j s(n - j).
TEST(LpcAnalysisTest, LevinsonFindsFirstOrderProcess) {
  const double a = 0.8;
  const std::vector<double> r = {1.0, a, a * a, a * a * a};
  const std::vector<double> k = Levinson(r, 3);
  ASSERT_EQ(k.size(), 3U);
  EXPECT_NEAR(k[0], -a, kTolerance);
  EXPECT_NEAR(k[1], 0.0, kTolerance);
  EXPECT_NEAR(k[2], 0.0, kTolerance);
}

TEST(LpcAnalysisTest, LevinsonClampsReflectionCoefficients) {
  const std::vector<double> r = {1.0, 1.0, 1.0};
  const std::vector<double> k = Levinson(r, 2);
  EXPECT_EQ(k[0], -kMaxReflection);
}

TEST(LpcAnalysisTest, LevinsonStopsWhenErrorIsNotPositive) {
  const std::vector<double> r = {0.0, 0.5, 0.25};
  EXPECT_EQ(Levinson(r, 2), (std::vector<double>{0.0, 0.0}));
}

TEST(LpcAnalysisTest, StepUpBuildsPredictorPolynomial) {
  EXPECT_EQ(StepUp(std::vector<double>{}), (std::vector<double>{1.0}));
  EXPECT_EQ(StepUp(std::vector<double>{0.5}), (std::vector<double>{1.0, 0.5}));
  // [1, k1 + k1 k2, k2]
  const std::vector<double> a = StepUp(std::vector<double>{0.5, 0.25});
  ASSERT_EQ(a.size(), 3U);
  EXPECT_DOUBLE_EQ(a[1], 0.5 + (0.5 * 0.25));
  EXPECT_DOUBLE_EQ(a[2], 0.25);
}

TEST(LpcAnalysisTest, PredictionErrorIsQuadraticForm) {
  // [1, a] on Toeplitz([r0, r1]): r0 + 2 a r1 + a^2 r0.
  const std::vector<double> a = {1.0, 0.5};
  const std::vector<double> r = {2.0, 1.0};
  EXPECT_DOUBLE_EQ(PredictionError(a, r), 2.0 + 1.0 + 0.5);
  EXPECT_DOUBLE_EQ(PredictionError(std::vector<double>{1.0}, r), 2.0);
}

TEST(LpcAnalysisTest, LogAreaRatioIsOddAndClamped) {
  EXPECT_EQ(LogAreaRatio(0.0), 0.0);
  EXPECT_NEAR(LogAreaRatio(0.5), std::log(1.0 / 3.0), kTolerance);
  EXPECT_NEAR(LogAreaRatio(-0.5), -LogAreaRatio(0.5), kTolerance);
  EXPECT_EQ(LogAreaRatio(1.5), LogAreaRatio(kMaxReflection));
}

}  // namespace
}  // namespace speak5220::core::dsp
