/*
 * File:        array_math_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of the NumPy-ordered sums, quantiles and windows
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/array_math.h"

#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

// numpy.sum([1e16, 1, -1e16, 1] * 5) is 1.0 by pairwise summation; a
// plain loop gives 10.0.
TEST(ArrayMathTest, PairwiseSumRoundsAsNumPyDoes) {
  std::vector<double> v;
  for (int i = 0; i < 5; ++i) {
    v.insert(v.end(), {1e16, 1.0, -1e16, 1.0});
  }
  EXPECT_EQ(PairwiseSum(v), 1.0);
}

TEST(ArrayMathTest, PairwiseSumAddsShortAndLongInputs) {
  EXPECT_EQ(PairwiseSum(std::vector<double>{}), 0.0);
  EXPECT_EQ(PairwiseSum(std::vector<double>{1.0, 2.0, 3.0}), 6.0);
  std::vector<double> ones(1000, 1.0);
  EXPECT_EQ(PairwiseSum(ones), 1000.0);
}

TEST(ArrayMathTest, MeanOfNothingIsZero) {
  EXPECT_EQ(Mean(std::vector<double>{}), 0.0);
  EXPECT_EQ(Mean(std::vector<double>{1.0, 2.0, 3.0, 6.0}), 3.0);
}

TEST(ArrayMathTest, DotMultipliesAndSums) {
  EXPECT_EQ(Dot(std::vector<double>{1.0, 2.0, 3.0},
                std::vector<double>{4.0, 5.0, 6.0}),
            32.0);
}

TEST(ArrayMathTest, MedianTakesMiddleOrMeanOfMiddlePair) {
  EXPECT_EQ(Median(std::vector<double>{5.0, 1.0, 3.0}), 3.0);
  EXPECT_EQ(Median(std::vector<double>{4.0, 1.0, 3.0, 2.0}), 2.5);
  EXPECT_EQ(Median(std::vector<double>{}), 0.0);
}

// numpy.percentile([1, 2, 3, 4], 95) and numpy.percentile([5, 1, 3], 5).
TEST(ArrayMathTest, PercentileInterpolatesLinearlyAsNumPy) {
  EXPECT_EQ(Percentile(std::vector<double>{1.0, 2.0, 3.0, 4.0}, 95.0),
            3.8499999999999996);
  EXPECT_EQ(Percentile(std::vector<double>{5.0, 1.0, 3.0}, 5.0), 1.2);
  EXPECT_EQ(Percentile(std::vector<double>{5.0, 1.0, 3.0}, 100.0), 5.0);
  EXPECT_EQ(Percentile(std::vector<double>{5.0, 1.0, 3.0}, 0.0), 1.0);
}

TEST(ArrayMathTest, WindowsMatchNumPy) {
  EXPECT_EQ(HannWindow(5), (std::vector<double>{0.0, 0.5, 1.0, 0.5, 0.0}));
  const std::vector<double> ham = HammingWindow(4);
  ASSERT_EQ(ham.size(), 4U);
  EXPECT_DOUBLE_EQ(ham[0], 0.08000000000000002);
  EXPECT_DOUBLE_EQ(ham[1], 0.7700000000000001);
  EXPECT_EQ(HannWindow(1), (std::vector<double>{1.0}));
  EXPECT_TRUE(HannWindow(0).empty());
}

TEST(ArrayMathTest, InnerHannWindowDropsTheZeroEnds) {
  EXPECT_EQ(InnerHannWindow(3), (std::vector<double>{0.5, 1.0, 0.5}));
}

}  // namespace
}  // namespace speak5220::core::dsp
