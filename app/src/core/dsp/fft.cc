/*
 * File:        fft.cc
 * Module:      dsp
 * Purpose:     Real FFTs: the interface and its pocketfft implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/fft.h"

// GCC at -O3 reports a false -Wmaybe-uninitialized inside pocketfft's
// templates once they are inlined here. pocketfft is often found in an
// implicit include directory, so it is not treated as a system header and its
// warnings are not suppressed; this one is silenced for the header only.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#endif
#include <pocketfft_hdronly.h>
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC diagnostic pop
#endif

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

std::vector<std::complex<double>> PocketFft::Forward(
    std::span<const double> input) {
  const std::size_t n = input.size();
  if (n == 0) {
    return {};
  }
  std::vector<std::complex<double>> bins(n / 2 + 1);
  const pocketfft::shape_t shape = {n};
  const pocketfft::stride_t stride_in = {sizeof(double)};
  const pocketfft::stride_t stride_out = {sizeof(std::complex<double>)};
  pocketfft::r2c(shape, stride_in, stride_out, /*axis=*/0, pocketfft::FORWARD,
                 input.data(), bins.data(), 1.0);
  return bins;
}

std::vector<double> PocketFft::Inverse(
    std::span<const std::complex<double>> bins, std::size_t n) {
  if (n == 0 || bins.size() != n / 2 + 1) {
    return {};
  }
  std::vector<double> output(n);
  const pocketfft::shape_t shape = {n};
  const pocketfft::stride_t stride_in = {sizeof(std::complex<double>)};
  const pocketfft::stride_t stride_out = {sizeof(double)};
  pocketfft::c2r(shape, stride_in, stride_out, /*axis=*/0, pocketfft::BACKWARD,
                 bins.data(), output.data(), 1.0 / static_cast<double>(n));
  return output;
}

}  // namespace speak5220::core::dsp
