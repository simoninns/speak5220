/*
 * File:        numpy_normal_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the NumPy PCG64 and standard normal generator
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/numpy_normal.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/dsp/array_math.h"
#include "gtest/gtest.h"

namespace speak5220::core::quality {
namespace {

// numpy.random.default_rng(1).standard_normal(20000) (NumPy 2.4.4), at a
// few indices; value 10530 is the largest in magnitude and comes from the
// ziggurat's tail.
struct Expected {
  std::size_t index;
  double value;
};
constexpr std::array<Expected, 9> kExpected = {{
    {.index = 0, .value = 0.345584192064786},
    {.index = 1, .value = 0.8216181435011584},
    {.index = 2, .value = 0.33043707618338714},
    {.index = 3, .value = -1.303157231604361},
    {.index = 4, .value = 0.9053558666731177},
    {.index = 100, .value = -0.651281012443394},
    {.index = 1000, .value = 0.19483956316952594},
    {.index = 5000, .value = 0.599476654534323},
    {.index = 19999, .value = 0.22883960321760555},
}};

std::vector<double> SeedOneValues(std::size_t n) {
  NumpyNormal normal = NumpyNormal::SeedOne();
  std::vector<double> v(n);
  for (double& x : v) {
    x = normal.Next();
  }
  return v;
}

TEST(NumpyNormalTest, SeedOneReproducesNumPysValues) {
  const std::vector<double> v = SeedOneValues(20000);
  for (const Expected& e : kExpected) {
    EXPECT_EQ(v[e.index], e.value) << e.index;
  }
}

TEST(NumpyNormalTest, SeedOneReproducesNumPysTailAndSum) {
  const std::vector<double> v = SeedOneValues(20000);
  std::size_t largest = 0;
  for (std::size_t i = 0; i < v.size(); ++i) {
    if (std::fabs(v[i]) > std::fabs(v[largest])) {
      largest = i;
    }
  }
  EXPECT_EQ(largest, 10530U);
  EXPECT_EQ(std::fabs(v[largest]), 3.9335548775304714);
  // numpy.sum, pairwise.
  EXPECT_EQ(dsp::PairwiseSum(v), -225.86881009402498);
}

TEST(NumpyNormalTest, UniformValuesAreInTheUnitInterval) {
  NumpyNormal normal = NumpyNormal::SeedOne();
  for (int i = 0; i < 1000; ++i) {
    const double u = normal.NextDouble();
    EXPECT_GE(u, 0.0);
    EXPECT_LT(u, 1.0);
  }
}

TEST(NumpyNormalTest, EqualStatesGiveEqualStreams) {
  NumpyNormal a({.high = 1, .low = 2}, {.high = 3, .low = 5});
  NumpyNormal b({.high = 1, .low = 2}, {.high = 3, .low = 5});
  NumpyNormal c({.high = 1, .low = 2}, {.high = 3, .low = 7});
  bool differs = false;
  for (int i = 0; i < 10; ++i) {
    const std::uint64_t va = a.NextUint64();
    EXPECT_EQ(va, b.NextUint64());
    differs = differs || va != c.NextUint64();
  }
  EXPECT_TRUE(differs);
}

}  // namespace
}  // namespace speak5220::core::quality
