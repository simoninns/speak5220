/*
 * File:        quantiser_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of K, pitch and energy quantisation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/quantiser.h"

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "core/dsp/lpc_analysis.h"
#include "core/lpc/coding_tables.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

// The index of each K table whose value is nearest zero.
std::vector<int> NearestZero(int count) {
  std::vector<int> out;
  for (int i = 0; i < count; ++i) {
    const auto table = lpc::KTable(i);
    std::size_t best = 0;
    for (std::size_t j = 1; j < table.size(); ++j) {
      if (std::abs(table[j]) < std::abs(table[best])) {
        best = j;
      }
    }
    out.push_back(static_cast<int>(best));
  }
  return out;
}

TEST(QuantiserTest, KValuesDecodeTheTables) {
  const std::vector<int> indices = {0, 31, 15};
  const std::vector<double> k = KValues(indices);
  ASSERT_EQ(k.size(), 3U);
  EXPECT_EQ(k[0], -501.0 / 512.0);
  EXPECT_EQ(k[1], 506.0 / 512.0);
  EXPECT_EQ(k[2], 368.0 / 512.0);
}

TEST(QuantiserTest, PredictionErrorOfFlatFilterIsSignalPower) {
  // White autocorrelation: the error is the sum of the squared predictor
  // coefficients times r[0].
  const std::vector<int> indices = NearestZero(4);
  const std::vector<double> r = {2.0, 0.0, 0.0, 0.0, 0.0};
  const std::vector<double> a = dsp::StepUp(KValues(indices));
  double sum = 0.0;
  for (const double v : a) {
    sum += v * v;
  }
  EXPECT_NEAR(QuantisedPredictionError(indices, r), 2.0 * sum, 1e-12);
}

TEST(QuantiserTest, WhiteSpectrumQuantisesToCoefficientsNearestZero) {
  const std::vector<double> k(10, 0.0);
  std::vector<double> r(11, 0.0);
  r[0] = 1.0;
  const QuantisedK q = QuantiseK(k, r);
  EXPECT_EQ(q.indices, NearestZero(10));
  EXPECT_DOUBLE_EQ(q.error, QuantisedPredictionError(q.indices, r));
}

TEST(QuantiserTest, CoordinateSearchLowersThePredictionError) {
  // Coefficients that do not suit a white spectrum: the search moves them
  // towards zero.
  const std::vector<double> k = {0.9, -0.6, 0.5, -0.4};
  const std::vector<double> r = {1.0, 0.0, 0.0, 0.0, 0.0};
  std::vector<int> nearest;
  for (int i = 0; i < 4; ++i) {
    const auto table = lpc::KTable(i);
    std::size_t best = 0;
    for (std::size_t j = 1; j < table.size(); ++j) {
      if (std::abs(dsp::LogAreaRatio(table[j] / 512.0) -
                   dsp::LogAreaRatio(k[static_cast<std::size_t>(i)])) <
          std::abs(dsp::LogAreaRatio(table[best] / 512.0) -
                   dsp::LogAreaRatio(k[static_cast<std::size_t>(i)]))) {
        best = j;
      }
    }
    nearest.push_back(static_cast<int>(best));
  }
  const QuantisedK q = QuantiseK(k, r);
  EXPECT_LT(q.error, QuantisedPredictionError(nearest, r));
  EXPECT_NE(q.indices, nearest);
}

TEST(QuantiserTest, PitchIndexFindsExactPeriods) {
  for (std::size_t i = 1; i < lpc::kPitchTable.size(); ++i) {
    EXPECT_EQ(PitchIndex(lpc::kPitchTable[i] + 1.0), static_cast<int>(i));
  }
}

TEST(QuantiserTest, PitchIndexClampsToTheTable) {
  EXPECT_EQ(PitchIndex(5.0), 1);
  EXPECT_EQ(PitchIndex(500.0), 63);
}

TEST(QuantiserTest, EnergyIndexIsNearestInDecibels) {
  EXPECT_EQ(EnergyIndex(0.0), 1);
  EXPECT_EQ(EnergyIndex(-3.0), 1);
  for (int i = 1; i <= 14; ++i) {
    EXPECT_EQ(EnergyIndex(lpc::kEnergyTable[static_cast<std::size_t>(i)]), i);
  }
  // Between 63 (index 12) and 85 (index 13) the boundary is their
  // geometric mean, about 73.2.
  EXPECT_EQ(EnergyIndex(73.0), 12);
  EXPECT_EQ(EnergyIndex(73.5), 13);
  EXPECT_EQ(EnergyIndex(1000.0), 14);
}

}  // namespace
}  // namespace speak5220::core::encoder
