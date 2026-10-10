/*
 * File:        lpc_analysis.cc
 * Module:      dsp
 * Purpose:     Linear-prediction analysis: autocorrelation, Levinson-Durbin,
 *              step-up and prediction error
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/lpc_analysis.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/dsp/array_math.h"

namespace speak5220::core::dsp {

std::vector<double> Autocorrelation(std::span<const double> segment,
                                    int order) {
  std::vector<double> r(static_cast<std::size_t>(order) + 1, 0.0);
  const std::size_t n = segment.size();
  for (std::size_t i = 0; i < r.size() && i < n; ++i) {
    r[i] = Dot(segment.first(n - i), segment.subspan(i));
  }
  return r;
}

std::vector<double> Levinson(std::span<const double> r, int order) {
  const auto p = static_cast<std::size_t>(order);
  assert(r.size() > p);
  std::vector<double> a(p + 1, 0.0);
  a[0] = 1.0;
  std::vector<double> next(p + 1, 0.0);
  std::vector<double> ks(p, 0.0);
  double err = r[0];
  for (std::size_t i = 1; i <= p; ++i) {
    if (err <= 0.0) {
      break;
    }
    // r[i] + a[1..i-1] . (r[i-1], ..., r[1])
    double dot = 0.0;
    for (std::size_t j = 1; j < i; ++j) {
      dot += a[j] * r[i - j];
    }
    const double acc = r[i] + dot;
    const double k = std::clamp(-acc / err, -kMaxReflection, kMaxReflection);
    ks[i - 1] = k;
    for (std::size_t j = 1; j <= i; ++j) {
      next[j] = a[j] + (k * a[i - j]);
    }
    for (std::size_t j = 1; j <= i; ++j) {
      a[j] = next[j];
    }
    a[i] = k;
    err *= 1.0 - (k * k);
  }
  return ks;
}

std::vector<double> StepUp(std::span<const double> k) {
  std::vector<double> a = {1.0};
  std::vector<double> next;
  for (const double ki : k) {
    a.push_back(0.0);
    const std::size_t n = a.size();
    next.assign(n, 0.0);
    for (std::size_t j = 0; j < n; ++j) {
      next[j] = a[j] + (ki * a[n - 1 - j]);
    }
    a.swap(next);
  }
  return a;
}

double PredictionError(std::span<const double> a, std::span<const double> r) {
  const std::size_t n = a.size();
  assert(r.size() >= n);
  // (a^T R) first, then the dot product with a, as numpy's a @ R @ a.
  double total = 0.0;
  for (std::size_t j = 0; j < n; ++j) {
    double column = 0.0;
    for (std::size_t i = 0; i < n; ++i) {
      column += a[i] * r[i > j ? i - j : j - i];
    }
    total += column * a[j];
  }
  return total;
}

double LogAreaRatio(double k) {
  const double c = std::clamp(k, -kMaxReflection, kMaxReflection);
  return std::log((1.0 - c) / (1.0 + c));
}

}  // namespace speak5220::core::dsp
