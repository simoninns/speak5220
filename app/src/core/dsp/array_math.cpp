/*
 * File:        array_math.cpp
 * Module:      dsp
 * Purpose:     Sums, means, quantiles and windows with NumPy's order of
 *              operations, so the encoder can match the Python reference
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/array_math.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

namespace speak5220::core::dsp {
namespace {

// numpy/_core/src/umath/loops_utils.h.src: the unrolling and block size
// of pairwise_sum.
constexpr std::size_t kUnroll = 8;
constexpr std::size_t kPairwiseBlock = 128;

// numpy.hanning and numpy.hamming: a + b cos(pi n / (m - 1)) for n = 1 - m,
// 3 - m, ..., m - 1.
std::vector<double> CosineWindow(int m, double a, double b) {
  if (m < 1) {
    return {};
  }
  if (m == 1) {
    return {1.0};
  }
  std::vector<double> w(static_cast<std::size_t>(m));
  const auto denominator = static_cast<double>(m - 1);
  for (int i = 0; i < m; ++i) {
    const auto n = static_cast<double>(1 - m + (2 * i));
    w[static_cast<std::size_t>(i)] =
        a + (b * std::cos(std::numbers::pi * n / denominator));
  }
  return w;
}

// numpy's _lerp(): a + (b - a) t, or b - (b - a)(1 - t) from t = 0.5, so
// the result is exact at both ends.
double Lerp(double a, double b, double t) {
  const double diff = b - a;
  return t >= 0.5 ? b - (diff * (1.0 - t)) : a + (diff * t);
}

}  // namespace

double PairwiseSum(std::span<const double> values) {
  const std::size_t n = values.size();
  if (n < kUnroll) {
    double sum = 0.0;
    for (const double v : values) {
      sum += v;
    }
    return sum;
  }
  if (n <= kPairwiseBlock) {
    std::array<double, kUnroll> r = {};
    for (std::size_t j = 0; j < kUnroll; ++j) {
      r[j] = values[j];
    }
    std::size_t i = kUnroll;
    for (; i < n - (n % kUnroll); i += kUnroll) {
      for (std::size_t j = 0; j < kUnroll; ++j) {
        r[j] += values[i + j];
      }
    }
    double sum =
        ((r[0] + r[1]) + (r[2] + r[3])) + ((r[4] + r[5]) + (r[6] + r[7]));
    for (; i < n; ++i) {
      sum += values[i];
    }
    return sum;
  }
  std::size_t half = n / 2;
  half -= half % kUnroll;
  return PairwiseSum(values.first(half)) + PairwiseSum(values.subspan(half));
}

double Mean(std::span<const double> values) {
  if (values.empty()) {
    return 0.0;
  }
  return PairwiseSum(values) / static_cast<double>(values.size());
}

double Dot(std::span<const double> a, std::span<const double> b) {
  const std::size_t n = std::min(a.size(), b.size());
  double sum = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    sum += a[i] * b[i];
  }
  return sum;
}

double Median(std::span<const double> values) {
  if (values.empty()) {
    return 0.0;
  }
  std::vector<double> sorted(values.begin(), values.end());
  std::ranges::sort(sorted);
  const std::size_t n = sorted.size();
  if (n % 2 == 1) {
    return sorted[n / 2];
  }
  return (sorted[(n / 2) - 1] + sorted[n / 2]) / 2.0;
}

double Percentile(std::span<const double> values, double percent) {
  if (values.empty()) {
    return 0.0;
  }
  std::vector<double> sorted(values.begin(), values.end());
  std::ranges::sort(sorted);
  const std::size_t n = sorted.size();
  const double q = percent / 100.0;
  // numpy's virtual index for the linear method: (n - 1) q.
  const double virtual_index = static_cast<double>(n - 1) * q;
  const auto last = static_cast<double>(n - 1);
  if (virtual_index >= last) {
    return sorted.back();
  }
  if (virtual_index < 0.0) {
    return sorted.front();
  }
  const double previous = std::floor(virtual_index);
  const auto index = static_cast<std::size_t>(previous);
  return Lerp(sorted[index], sorted[index + 1], virtual_index - previous);
}

std::vector<double> HannWindow(int m) {
  constexpr double kHann = 0.5;
  return CosineWindow(m, kHann, kHann);
}

std::vector<double> HammingWindow(int m) {
  constexpr double kHammingA = 0.54;
  constexpr double kHammingB = 0.46;
  return CosineWindow(m, kHammingA, kHammingB);
}

std::vector<double> InnerHannWindow(int m) {
  std::vector<double> w = HannWindow(m + 2);
  if (w.size() < 2) {
    return {};
  }
  return {w.begin() + 1, w.end() - 1};
}

}  // namespace speak5220::core::dsp
