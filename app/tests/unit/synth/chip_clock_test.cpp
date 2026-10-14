/*
 * File:        chip_clock_test.cpp
 * Module:      synth tests
 * Purpose:     Unit tests of the chip clock rates offered and the sample
 *              rate each gives
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/chip_clock.h"

#include <algorithm>
#include <cstddef>
#include <optional>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace speak5220::core::synth {
namespace {

TEST(ChipClockTest, NominalClockGives8Kilohertz) {
  EXPECT_EQ(ChipSampleRate(kNominalClockHz), 8000);
}

TEST(ChipClockTest, RatesRunSlowestFirstAroundTheNominalClock) {
  const auto clocks = ChipClocks();
  ASSERT_FALSE(clocks.empty());
  EXPECT_EQ(clocks.front().hz, 560000);
  EXPECT_EQ(clocks.back().hz, 720000);
  EXPECT_TRUE(std::ranges::is_sorted(
      clocks, {}, [](const ChipClockInfo& info) { return info.hz; }));
  EXPECT_TRUE(std::ranges::any_of(clocks, [](const ChipClockInfo& info) {
    return info.hz == kNominalClockHz;
  }));
}

TEST(ChipClockTest, EveryRateIsAWholeNumberOfSamples) {
  for (const ChipClockInfo& info : ChipClocks()) {
    EXPECT_EQ(info.hz % kClocksPerSample, 0) << info.label;
    EXPECT_FALSE(info.label.empty());
  }
}

TEST(ChipClockTest, FromHzAcceptsOnlyTheRatesOffered) {
  for (const ChipClockInfo& info : ChipClocks()) {
    EXPECT_EQ(ChipClockFromHz(info.hz), info.hz);
  }
  EXPECT_EQ(ChipClockFromHz(650000), std::nullopt);
  EXPECT_EQ(ChipClockFromHz(0), std::nullopt);
}

}  // namespace
}  // namespace speak5220::core::synth
