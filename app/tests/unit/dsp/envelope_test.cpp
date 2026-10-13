/*
 * File:        envelope_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of the shared level and gain envelopes: the
 *              centred RMS, and dips and peaks spread into ramps
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/envelope.h"

#include <cmath>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

TEST(EnvelopeTest, SamplesOfRoundsAndIsAtLeastOne) {
  EXPECT_EQ(SamplesOf(20.0, 8000), 160U);
  EXPECT_EQ(SamplesOf(0.01, 8000), 1U);
  EXPECT_EQ(SamplesOf(1.0, 8000), 8U);
}

TEST(EnvelopeTest, CentredRmsIsTheWindowsRms) {
  // 2 ms at 8 kHz: 16 samples, so 8 either side.
  std::vector<double> x(100, 0.0);
  x[50] = 4.0;
  const std::vector<double> level = CentredRms(x, 8000, 2.0);
  ASSERT_EQ(level.size(), x.size());
  EXPECT_DOUBLE_EQ(level[50], std::sqrt(16.0 / 17.0));
  EXPECT_DOUBLE_EQ(level[42], std::sqrt(16.0 / 17.0));
  EXPECT_EQ(level[41], 0.0);
  // Shortened at the ends.
  const std::vector<double> ones(10, 1.0);
  for (const double value : CentredRms(ones, 8000, 2.0)) {
    EXPECT_DOUBLE_EQ(value, 1.0);
  }
}

TEST(EnvelopeTest, SpreadDipsRampsUpAfterAndBefore) {
  std::vector<double> gain_db(9, 0.0);
  gain_db[4] = -6.0;
  SpreadDips(gain_db, 2.0, 3.0);
  EXPECT_EQ(gain_db, (std::vector<double>{0.0, 0.0, 0.0, -3.0, -6.0, -4.0, -2.0,
                                          0.0, 0.0}));
}

TEST(EnvelopeTest, SpreadPeaksRampsDownAfterAndBefore) {
  std::vector<double> gain_db(9, -10.0);
  gain_db[4] = 0.0;
  SpreadPeaks(gain_db, 4.0, 5.0);
  EXPECT_EQ(gain_db, (std::vector<double>{-10.0, -10.0, -10.0, -5.0, 0.0, -4.0,
                                          -8.0, -10.0, -10.0}));
}

TEST(EnvelopeTest, DbToGain) {
  const std::vector<double> gain = DbToGain(std::vector<double>{0.0, -20.0});
  EXPECT_DOUBLE_EQ(gain[0], 1.0);
  EXPECT_DOUBLE_EQ(gain[1], 0.1);
}

}  // namespace
}  // namespace speak5220::core::dsp
