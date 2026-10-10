/*
 * File:        iir_filter_test.cc
 * Module:      dsp tests
 * Purpose:     Unit tests of second-order-section filtering, filtfilt and
 *              FIR filtering against small SciPy cases
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/iir_filter.h"

#include <array>
#include <cstddef>
#include <vector>

#include "core/dsp/filter_constants.h"
#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

constexpr double kTolerance = 1e-12;

// A one-pole section y[n] = x[n] + 0.5 y[n-1]: its impulse response
// halves each sample.
constexpr SosSection kHalving = {.b = {1.0, 0.0, 0.0}, .a = {-0.5, 0.0}};

TEST(IirFilterTest, SectionImpulseResponseFollowsItsPole) {
  std::vector<double> x = {1.0, 0.0, 0.0, 0.0};
  std::array<double, 2> state = {};
  const std::array<SosSection, 1> sections = {kHalving};
  SosFilterInPlace(sections, state, x);
  EXPECT_EQ(x, (std::vector<double>{1.0, 0.5, 0.25, 0.125}));
}

TEST(IirFilterTest, SectionStateCarriesAcrossCalls) {
  const std::array<SosSection, 1> sections = {kHalving};
  std::array<double, 2> state = {};
  std::vector<double> first = {1.0, 0.0};
  std::vector<double> second = {0.0, 0.0};
  SosFilterInPlace(sections, state, first);
  SosFilterInPlace(sections, state, second);
  EXPECT_EQ(second, (std::vector<double>{0.25, 0.125}));
}

// scipy.signal.sosfiltfilt(butter(2, 60/4000, "highpass", output="sos"), x)
// for this x, from SciPy 1.17.1.
TEST(IirFilterTest, FiltFiltMatchesSciPy) {
  const std::vector<double> x = {0.0, 0.5,  1.0, 0.25, -0.75, -1.0, -0.5,  0.0,
                                 0.5, 0.75, 0.5, 0.0,  -0.25, -0.5, -0.25, 0.0};
  const std::vector<double> expected = {
      0.26676229400031126,  0.7433361357967688,  1.2193031653560058,
      0.4446478124019274,   -0.5806491186659036, -0.8566083993222536,
      -0.3832494760954511,  0.08941081422657957, 0.5613572965830526,
      0.7825744335425519,   0.5030445360562485,  -0.027252865546962515,
      -0.30834018179795025, -0.5902400723317369, -0.3729739044513609,
      -0.15656105588557367};
  const std::vector<double> y = SosFiltFilt(kHighPass60Hz, x);
  ASSERT_EQ(y.size(), expected.size());
  for (std::size_t i = 0; i < y.size(); ++i) {
    EXPECT_NEAR(y[i], expected[i], kTolerance) << i;
  }
}

TEST(IirFilterTest, FiltFiltPassesDirectCurrentThroughLowPass) {
  const std::vector<double> x(100, 0.25);
  for (const double v : SosFiltFilt(kLowPass1kHz, x)) {
    EXPECT_NEAR(v, 0.25, kTolerance);
  }
}

TEST(IirFilterTest, FiltFiltRemovesDirectCurrentThroughHighPass) {
  const std::vector<double> x(100, 0.25);
  for (const double v : SosFiltFilt(kHighPass60Hz, x)) {
    EXPECT_NEAR(v, 0.0, kTolerance);
  }
}

TEST(IirFilterTest, FiltFiltReturnsInputNoLongerThanItsPadding) {
  const std::vector<double> x = {1.0, -1.0, 0.5};
  EXPECT_EQ(SosFiltFilt(kLowPass1kHz, x), x);
}

// scipy.signal.lfilter([1, -0.5, 0.25], [1], [1, 2, 3, 4]).
TEST(IirFilterTest, FirFilterMatchesLfilter) {
  const std::vector<double> b = {1.0, -0.5, 0.25};
  EXPECT_EQ(FirFilter(b, std::vector<double>{1.0, 2.0, 3.0, 4.0}),
            (std::vector<double>{1.0, 1.5, 2.25, 3.0}));
}

TEST(IirFilterTest, FirFilterWithNoTapsGivesZeros) {
  EXPECT_EQ(FirFilter({}, std::vector<double>{1.0, 2.0}),
            (std::vector<double>{0.0, 0.0}));
}

}  // namespace
}  // namespace speak5220::core::dsp
