/*
 * File:        iir_filter.h
 * Module:      dsp
 * Purpose:     Second-order-section IIR filtering, zero-phase filtfilt and
 *              FIR filtering, with SciPy's conventions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_IIR_FILTER_H_
#define SPEAK5220_CORE_DSP_IIR_FILTER_H_

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

// One second-order section, normalised so a0 = 1:
// H(z) = (b0 + b1 z^-1 + b2 z^-2) / (1 + a1 z^-1 + a2 z^-2). `zi` is the
// section's steady-state initial condition for a unit step, as
// scipy.signal.sosfilt_zi() gives it (it carries the gain of the sections
// before it).
struct SosSection {
  std::array<double, 3> b = {};
  std::array<double, 2> a = {};
  std::array<double, 2> zi = {};
};

// A cascade of second-order sections, with the length of the odd extension
// scipy.signal.sosfiltfilt() pads each end with. A plain value type; the
// encoder's filters are constants in filter_constants.h.
template <std::size_t N>
struct SosFilter {
  std::array<SosSection, N> sections = {};
  int pad_length = 0;
};

// Filters `x` in place through `sections`, starting from the state
// `state` (two values per section), which is updated. Direct form II
// transposed, in the order of operations of scipy.signal.sosfilt().
// O(n x sections).
void SosFilterInPlace(std::span<const SosSection> sections,
                      std::span<double> state, std::span<double> x);

// Zero-phase filtering: forwards then backwards through `sections`, with
// an odd extension of `pad_length` samples at each end and the steady-state
// initial conditions, as scipy.signal.sosfiltfilt() does with its default
// padding. `x` must be longer than `pad_length`; a shorter input is
// returned unchanged. O(n x sections).
std::vector<double> SosFiltFilt(std::span<const SosSection> sections,
                                int pad_length, std::span<const double> x);

template <std::size_t N>
std::vector<double> SosFiltFilt(const SosFilter<N>& filter,
                                std::span<const double> x) {
  return SosFiltFilt(filter.sections, filter.pad_length, x);
}

// FIR filtering y[n] = sum b[k] x[n - k], keeping the first x.size()
// outputs, as scipy.signal.lfilter(b, [1.0], x) does (by numpy.convolve).
// O(n x taps).
std::vector<double> FirFilter(std::span<const double> b,
                              std::span<const double> x);

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_IIR_FILTER_H_
