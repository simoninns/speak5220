/*
 * File:        coding_tables_test.cc
 * Module:      lpc tests
 * Purpose:     Unit tests of the coding tables against the datasheet's
 *              stated properties
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/lpc/coding_tables.h"

#include <algorithm>
#include <cstddef>

#include "gtest/gtest.h"

namespace speak5220::core::lpc {
namespace {

// Re-implements test_tms5220enc.py Arithmetic.testTables.
TEST(CodingTablesTest, ChirpEnergyIs65546) {
  // speak5220 datasheet §12.3: the chirp's energy.
  int sum = 0;
  for (const int c : kChirpTable) {
    sum += c * c;
  }
  EXPECT_EQ(sum, 65546);
  EXPECT_EQ(kChirpTable.size(), 52U);
}

TEST(CodingTablesTest, KTablesHaveOneEntryPerCode) {
  for (int k = 0; k < kKCount; ++k) {
    EXPECT_EQ(KTable(k).size(),
              std::size_t{1} << kKBits.at(static_cast<std::size_t>(k)))
        << "K" << (k + 1);
  }
}

TEST(CodingTablesTest, KTablesAreSorted) {
  for (int k = 0; k < kKCount; ++k) {
    EXPECT_TRUE(std::is_sorted(KTable(k).begin(), KTable(k).end()))
        << "K" << (k + 1);
  }
}

TEST(CodingTablesTest, PitchPeriodIsValuePlusOne) {
  // speak5220 datasheet Table 8-5.
  EXPECT_EQ(kPitchTable[13] + 1, 28);
  EXPECT_EQ(kPitchTable[63] + 1, 160);
}

TEST(CodingTablesTest, ParameterValueReadsEachTable) {
  EXPECT_EQ(ParameterValue(kEnergyParameter, 14), 114);
  EXPECT_EQ(ParameterValue(kEnergyParameter, 15), 0);
  EXPECT_EQ(ParameterValue(kPitchParameter, 1), 15);
  EXPECT_EQ(ParameterValue(kFirstKParameter, 0), -501);
  EXPECT_EQ(ParameterValue(kFirstKParameter + 9, 7), 307);
}

}  // namespace
}  // namespace speak5220::core::lpc
