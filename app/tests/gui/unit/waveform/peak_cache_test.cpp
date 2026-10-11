/*
 * File:        peak_cache_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of the waveform's peak cache against a plain scan
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/peak_cache.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

// Deterministic samples in [-1, 1): a linear congruential generator, so
// the values are the same on every platform.
std::vector<float> Samples(std::size_t n) {
  std::vector<float> samples;
  samples.reserve(n);
  std::uint32_t state = 12345;
  for (std::size_t i = 0; i < n; ++i) {
    state = (state * 1664525U) + 1013904223U;
    samples.push_back((static_cast<float>(state >> 8) / 8388608.0F) - 1.0F);
  }
  return samples;
}

Peak Scan(const std::vector<float>& samples, std::int64_t first,
          std::int64_t last) {
  const auto begin = samples.begin() + first;
  const auto end = samples.begin() + last;
  return {.low = *std::min_element(begin, end),
          .high = *std::max_element(begin, end)};
}

TEST(PeakCacheTest, EveryRangeMatchesAPlainScan) {
  // Not a whole number of blocks at any level.
  const std::vector<float> samples = Samples(5000);
  const PeakCache cache(samples);
  ASSERT_EQ(cache.Size(), samples.size());
  const auto n = static_cast<std::int64_t>(samples.size());
  for (std::int64_t first = 0; first < n; first += 37) {
    for (std::int64_t last = first + 1; last <= n; last += 53) {
      EXPECT_EQ(cache.Range(first, last), Scan(samples, first, last))
          << "[" << first << ", " << last << ")";
    }
    EXPECT_EQ(cache.Range(first, n), Scan(samples, first, n));
  }
}

TEST(PeakCacheTest, SingleSampleIsItsOwnPeak) {
  const std::vector<float> samples = Samples(100);
  const PeakCache cache(samples);
  EXPECT_EQ(cache.Range(42, 43),
            (Peak{.low = samples[42], .high = samples[42]}));
}

TEST(PeakCacheTest, RangeIsClampedToTheSamples) {
  const std::vector<float> samples = Samples(300);
  const PeakCache cache(samples);
  EXPECT_EQ(cache.Range(-50, 10), Scan(samples, 0, 10));
  EXPECT_EQ(cache.Range(290, 900), Scan(samples, 290, 300));
  EXPECT_EQ(cache.Range(-10, 0), Peak{});
  EXPECT_EQ(cache.Range(300, 310), Peak{});
  EXPECT_EQ(cache.Range(20, 20), Peak{});
}

TEST(PeakCacheTest, EmptyCacheHasNoPeaks) {
  const PeakCache cache;
  EXPECT_EQ(cache.Size(), 0U);
  EXPECT_EQ(cache.Range(0, 100), Peak{});
}

}  // namespace
}  // namespace speak5220::app
