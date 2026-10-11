/*
 * File:        pesq_fft_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of PESQ's single-precision radix-2 FFT
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pesq_fft.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::quality {
namespace {

TEST(PesqFftTest, ImpulseHasAFlatSpectrum) {
  constexpr int kN = 16;
  std::vector<float> x(kN + 2, 0.0F);
  x[0] = 1.0F;
  RealFft(x, kN);
  for (int k = 0; k <= kN / 2; ++k) {
    EXPECT_FLOAT_EQ(x[static_cast<std::size_t>(2 * k)], 1.0F) << k;
    EXPECT_FLOAT_EQ(x[static_cast<std::size_t>((2 * k) + 1)], 0.0F) << k;
  }
}

TEST(PesqFftTest, MatchesTheDiscreteFourierTransform) {
  constexpr int kN = 64;
  std::vector<float> x(kN + 2, 0.0F);
  for (int i = 0; i < kN; ++i) {
    x[static_cast<std::size_t>(i)] =
        static_cast<float>(std::sin(0.3 * i) + 0.5 * std::cos(1.7 * i));
  }
  const std::vector<float> input = x;
  RealFft(x, kN);
  for (int k = 0; k <= kN / 2; ++k) {
    double re = 0.0;
    double im = 0.0;
    for (int i = 0; i < kN; ++i) {
      const double a = 2 * std::numbers::pi * k * i / kN;
      re += input[static_cast<std::size_t>(i)] * std::cos(a);
      im -= input[static_cast<std::size_t>(i)] * std::sin(a);
    }
    EXPECT_NEAR(x[static_cast<std::size_t>(2 * k)], re, 1e-4) << k;
    EXPECT_NEAR(x[static_cast<std::size_t>((2 * k) + 1)], im, 1e-4) << k;
  }
}

TEST(PesqFftTest, InverseUndoesForward) {
  constexpr int kN = 128;
  std::vector<float> x(kN + 2, 0.0F);
  for (int i = 0; i < kN; ++i) {
    x[static_cast<std::size_t>(i)] = static_cast<float>((i * 37) % 11) - 5.0F;
  }
  const std::vector<float> input = x;
  RealFft(x, kN);
  RealIfft(x, kN);
  for (int i = 0; i < kN; ++i) {
    EXPECT_NEAR(x[static_cast<std::size_t>(i)],
                input[static_cast<std::size_t>(i)], 1e-5)
        << i;
  }
}

TEST(PesqFftTest, ComplexInverseScalesByTheSize) {
  constexpr int kN = 8;
  std::vector<float> x(2 * static_cast<std::size_t>(kN), 0.0F);
  x[2] = 1.0F;  // bin 1, real part
  ComplexIfft(x, kN);
  for (int i = 0; i < kN; ++i) {
    const double a = 2 * std::numbers::pi * i / kN;
    EXPECT_NEAR(x[static_cast<std::size_t>(2 * i)], std::cos(a) / kN, 1e-7);
    EXPECT_NEAR(x[static_cast<std::size_t>((2 * i) + 1)], std::sin(a) / kN,
                1e-7);
  }
}

TEST(PesqFftTest, NextPowerOfTwoRoundsUp) {
  EXPECT_EQ(NextPowerOfTwo(0), 1);
  EXPECT_EQ(NextPowerOfTwo(1), 1);
  EXPECT_EQ(NextPowerOfTwo(5), 8);
  EXPECT_EQ(NextPowerOfTwo(512), 512);
  EXPECT_EQ(NextPowerOfTwo(513), 1024);
}

// The correlation peaks where x2 is x1 delayed: entry x1.size() - 1 is the
// zero lag.
TEST(PesqFftTest, CrossCorrelationPeaksAtTheDelay) {
  const std::vector<float> x1 = {0, 1, 3, -2, 1, 0, 0, 0};
  const std::vector<float> x2 = {0, 0, 0, 1, 3, -2, 1, 0};
  const std::vector<float> y = CrossCorrelate(x1, x2);
  ASSERT_EQ(y.size(), x1.size() + x2.size() - 1);
  std::size_t peak = 0;
  for (std::size_t i = 0; i < y.size(); ++i) {
    if (y[i] > y[peak]) {
      peak = i;
    }
  }
  EXPECT_EQ(static_cast<int>(peak) - static_cast<int>(x1.size() - 1), 2);
  EXPECT_NEAR(y[peak], 1 + 9 + 4 + 1, 1e-4);
}

}  // namespace
}  // namespace speak5220::core::quality
