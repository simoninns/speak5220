/*
 * File:        iir_filter.cc
 * Module:      dsp
 * Purpose:     Second-order-section IIR filtering, zero-phase filtfilt and
 *              FIR filtering, with SciPy's conventions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/iir_filter.h"

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

void SosFilterInPlace(std::span<const SosSection> sections,
                      std::span<double> state, std::span<double> x) {
  assert(state.size() == 2 * sections.size());
  for (double& sample : x) {
    double x_cur = sample;
    for (std::size_t s = 0; s < sections.size(); ++s) {
      const SosSection& section = sections[s];
      double& z0 = state[2 * s];
      double& z1 = state[(2 * s) + 1];
      const double x_new = (section.b[0] * x_cur) + z0;
      z0 = (section.b[1] * x_cur) - (section.a[0] * x_new) + z1;
      z1 = (section.b[2] * x_cur) - (section.a[1] * x_new);
      x_cur = x_new;
    }
    sample = x_cur;
  }
}

std::vector<double> SosFiltFilt(std::span<const SosSection> sections,
                                int pad_length, std::span<const double> x) {
  const auto pad = static_cast<std::size_t>(std::max(pad_length, 0));
  const std::size_t n = x.size();
  if (n <= pad || n < 2) {
    return {x.begin(), x.end()};
  }
  // scipy.signal._arraytools.odd_ext: 2 x[0] - x[pad..1] before the
  // signal, 2 x[n-1] - x[n-2..n-1-pad] after it.
  std::vector<double> ext;
  ext.reserve(n + (2 * pad));
  for (std::size_t i = pad; i >= 1; --i) {
    ext.push_back((2.0 * x[0]) - x[i]);
  }
  ext.insert(ext.end(), x.begin(), x.end());
  for (std::size_t i = 1; i <= pad; ++i) {
    ext.push_back((2.0 * x[n - 1]) - x[n - 1 - i]);
  }

  std::vector<double> state(2 * sections.size());
  const auto start_from = [&](double value) {
    for (std::size_t s = 0; s < sections.size(); ++s) {
      state[2 * s] = sections[s].zi[0] * value;
      state[(2 * s) + 1] = sections[s].zi[1] * value;
    }
  };
  start_from(ext.front());
  SosFilterInPlace(sections, state, ext);
  std::ranges::reverse(ext);
  start_from(ext.front());
  SosFilterInPlace(sections, state, ext);
  std::ranges::reverse(ext);
  const auto first = ext.begin() + static_cast<std::ptrdiff_t>(pad);
  return {first, first + static_cast<std::ptrdiff_t>(n)};
}

std::vector<double> FirFilter(std::span<const double> b,
                              std::span<const double> x) {
  std::vector<double> y(x.size());
  const std::size_t taps = b.size();
  if (taps == 0) {
    return y;
  }
  for (std::size_t n = 0; n < x.size(); ++n) {
    // numpy.convolve sums the overlap in the input's order: from x[n - k]
    // for the last tap up to x[n] for the first.
    const std::size_t top = std::min(taps - 1, n);
    double sum = 0.0;
    bool first = true;
    for (std::size_t k = top + 1; k-- > 0;) {
      const double term = b[k] * x[n - k];
      sum = first ? term : sum + term;
      first = false;
    }
    y[n] = sum;
  }
  return y;
}

}  // namespace speak5220::core::dsp
