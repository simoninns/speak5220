/*
 * File:        chip_arithmetic_test.cc
 * Module:      emulator tests
 * Purpose:     Unit tests of the Booth multiplier, the 14-bit wrap, the
 *              noise register and the DAC model
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/chip_arithmetic.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <random>

#include "gtest/gtest.h"

namespace speak5220::core::emulator {
namespace {

struct MultiplierExample {
  int k;
  std::int32_t m;
  std::array<int, 5> digits;
  std::int32_t product;
};

// speak5220 datasheet §8.8, the multiplier examples: K, M, digits d0..d4
// and the product. The first row is "K = 0, M any"; 12345 stands for any.
constexpr std::array<MultiplierExample, 9> kExamples = {{
    {.k = 0, .m = 12345, .digits = {0, 0, 0, 0, 0}, .product = 1},
    {.k = 33, .m = 192, .digits = {1, 0, -2, 1, 0}, .product = 13},
    {.k = 114, .m = 4096, .digits = {-2, 1, -1, 2, 0}, .product = 911},
    {.k = -18, .m = 3, .digits = {-2, 0, -1, 0, 0}, .product = -1},
    {.k = -18, .m = -3, .digits = {-2, 0, -1, 0, 0}, .product = 1},
    {.k = 506, .m = 1000, .digits = {-2, -1, 0, 0, 2}, .product = 989},
    {.k = -501, .m = -777, .digits = {-1, -1, 1, 0, -2}, .product = 761},
    {.k = 409, .m = -8192, .digits = {1, -2, 2, -2, 2}, .product = -6543},
    {.k = -256, .m = 8191, .digits = {0, 0, 0, 0, -1}, .product = -4095},
}};

TEST(ChipArithmeticTest, EveryMultiplierExampleRowPasses) {
  for (const MultiplierExample& row : kExamples) {
    SCOPED_TRACE(::testing::Message() << "K=" << row.k << " M=" << row.m);
    EXPECT_EQ(BoothDigits(row.k), row.digits);
    EXPECT_EQ(Booth(row.m, row.k), row.product);
    EXPECT_EQ(BoothClosedForm(row.m, row.k), row.product);
  }
}

// The examples hold at compile time too.
static_assert(Booth(4096, 114) == 911);
static_assert(Booth(-8192, 409) == -6543);

TEST(ChipArithmeticTest, ZeroCoefficientGivesForcedOne) {
  EXPECT_EQ(Booth(0, 0), 1);
  EXPECT_EQ(Booth(5000, 0), 1);
  EXPECT_EQ(Booth(1234, 0), 1);
}

// Re-implements test_tms5220enc.py Arithmetic.testBoothFastMatches.
TEST(ChipArithmeticTest, ClosedFormMatchesBoothDigits) {
  std::mt19937 rng(4);
  std::uniform_int_distribution<int> k_values(-512, 511);
  std::uniform_int_distribution<std::int32_t> m_values(-8192, 8191);
  for (int i = 0; i < 20000; ++i) {
    const int k = k_values(rng);
    const std::int32_t m = m_values(rng);
    ASSERT_EQ(Booth(m, k), BoothClosedForm(m, k)) << "K=" << k << " M=" << m;
  }
}

TEST(ChipArithmeticTest, MultiplierApproximatesProductOver512) {
  // Within a couple of LSB of M*K/512 (test_tms5220.c test_arith).
  std::int32_t worst = 0;
  for (std::int32_t m = -8192; m < 8192; m += 37) {
    for (int k = -512; k < 512; k += 7) {
      const auto exact =
          static_cast<std::int32_t>((static_cast<std::int64_t>(m) * k) / 512);
      worst = std::max(worst, std::abs(Booth(m, k) - exact));
    }
  }
  EXPECT_LE(worst, 3);
}

TEST(ChipArithmeticTest, ShiftRoundsDown) {
  EXPECT_EQ(Asr(-1, 4), -1);
  EXPECT_EQ(Asr(-17, 4), -2);
  EXPECT_EQ(Asr(16, 4), 1);
}

TEST(ChipArithmeticTest, WrapsToFourteenBits) {
  EXPECT_EQ(Wrap14(8191), 8191);
  EXPECT_EQ(Wrap14(8192), -8192);
  EXPECT_EQ(Wrap14(-8193), 8191);
  EXPECT_EQ(Wrap14(-8192), -8192);
}

TEST(ChipArithmeticTest, NoiseRegisterHasPeriod8191) {
  // speak5220 datasheet §8.6.
  std::uint32_t n = NoiseStep(1);
  int period = 1;
  while (n != 1 && period <= 8192) {
    n = NoiseStep(n);
    ++period;
  }
  EXPECT_EQ(period, 8191);
}

TEST(ChipArithmeticTest, DacIdleCurrentIsAbout600Microamps) {
  // speak5220 datasheet §8.2: idle code -1 into 996 ohm.
  const double idle = DacCurrentMicroamps(-1, DacLoad::k996Ohm);
  EXPECT_GT(idle, 590.0);
  EXPECT_LT(idle, 610.0);
  EXPECT_DOUBLE_EQ(DacCurrentMicroamps(127, DacLoad::k996Ohm), 0.0);
  EXPECT_DOUBLE_EQ(DacCurrentMicroamps(1000, DacLoad::k482Ohm), 0.0);
}

}  // namespace
}  // namespace speak5220::core::emulator
