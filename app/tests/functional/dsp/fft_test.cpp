/*
 * File:        fft_test.cpp
 * Module:      dsp tests
 * Purpose:     Functional tests of the pocketfft adapter against a direct
 *              DFT
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/fft.h"

#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <random>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

// The first n/2 + 1 bins of the DFT, computed directly.
std::vector<std::complex<double>> DirectDft(const std::vector<double>& x) {
  const std::size_t n = x.size();
  std::vector<std::complex<double>> bins(n / 2 + 1);
  for (std::size_t k = 0; k < bins.size(); ++k) {
    std::complex<double> sum = 0.0;
    for (std::size_t t = 0; t < n; ++t) {
      const double angle = -2.0 * std::numbers::pi *
                           static_cast<double>(k * t % n) /
                           static_cast<double>(n);
      sum += x[t] * std::polar(1.0, angle);
    }
    bins[k] = sum;
  }
  return bins;
}

std::vector<double> RandomSignal(std::size_t n, unsigned seed) {
  std::mt19937 rng(seed);
  std::uniform_real_distribution<double> value(-1.0, 1.0);
  std::vector<double> x(n);
  for (double& v : x) {
    v = value(rng);
  }
  return x;
}

class PocketFftTest : public ::testing::TestWithParam<std::size_t> {};

// Phase 1 task 7: Fft matches a direct DFT, for power-of-two and other
// lengths.
TEST_P(PocketFftTest, ForwardMatchesDirectDft) {
  const std::vector<double> x = RandomSignal(GetParam(), 1);
  PocketFft fft;
  const std::vector<std::complex<double>> bins = fft.Forward(x);
  const std::vector<std::complex<double>> expected = DirectDft(x);
  ASSERT_EQ(bins.size(), expected.size());
  for (std::size_t k = 0; k < bins.size(); ++k) {
    EXPECT_NEAR(bins[k].real(), expected[k].real(), 1e-9) << "bin " << k;
    EXPECT_NEAR(bins[k].imag(), expected[k].imag(), 1e-9) << "bin " << k;
  }
}

TEST_P(PocketFftTest, InverseUndoesForward) {
  const std::vector<double> x = RandomSignal(GetParam(), 2);
  PocketFft fft;
  const std::vector<double> back = fft.Inverse(fft.Forward(x), x.size());
  ASSERT_EQ(back.size(), x.size());
  for (std::size_t i = 0; i < x.size(); ++i) {
    EXPECT_NEAR(back[i], x[i], 1e-12) << "sample " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(Lengths, PocketFftTest,
                         ::testing::Values(1, 2, 200, 256, 241, 4096));

TEST(PocketFftEdgeTest, EmptyInputGivesNoBins) {
  PocketFft fft;
  EXPECT_TRUE(fft.Forward({}).empty());
  EXPECT_TRUE(fft.Inverse({}, 0).empty());
}

}  // namespace
}  // namespace speak5220::core::dsp
