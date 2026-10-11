/*
 * File:        pesq_fft.cpp
 * Module:      quality
 * Purpose:     The single-precision radix-2 FFT that PESQ's reference
 *              implementation computes with, in its order of operations
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pesq_fft.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <utility>
#include <vector>

namespace speak5220::core::quality {

namespace {

// The reference's 2π, as a float literal: its twiddle angles are computed
// in single precision.
constexpr float kTwoPiFloat = 6.283185307179586F;

// The butterfly order and the twiddle factors for one size.
struct FftTables {
  std::vector<std::size_t> butter;
  std::vector<float> phi;  // cos, sin interleaved
};

FftTables MakeTables(std::size_t n) {
  FftTables t;
  t.butter.assign(n >> 1U, 0);
  t.phi.resize(n);
  const auto n_float = static_cast<float>(n);
  for (std::size_t c = 0; c < (n >> 1U); ++c) {
    const float theta = (kTwoPiFloat * static_cast<float>(c)) / n_float;
    t.phi[2 * c] = static_cast<float>(std::cos(static_cast<double>(theta)));
    t.phi[(2 * c) + 1] =
        static_cast<float>(std::sin(static_cast<double>(theta)));
  }
  std::size_t l = 1;
  for (std::size_t k = n >> 2U; k >= 1; k >>= 1U) {
    for (std::size_t c = 0; c < l; ++c) {
      t.butter[c + l] = t.butter[c] + k;
    }
    l <<= 1U;
  }
  return t;
}

// The tables for `n`, cached per thread for the last size used.
const FftTables& TablesFor(std::size_t n) {
  thread_local std::size_t cached_n = 0;
  thread_local FftTables cached;
  if (cached_n != n) {
    cached = MakeTables(n);
    cached_n = n;
  }
  return cached;
}

// The butterflies of the forward (sign +1) or inverse (sign -1) transform,
// then the bit-reversal swap. The two directions differ only in the signs
// of the imaginary twiddle terms, written out as the reference does so
// the floating-point evaluation order is the same.
void Transform(std::span<float> x, std::size_t n, bool inverse) {
  if (n <= 1) {
    return;
  }
  assert(x.size() >= 2 * n);
  const FftTables& t = TablesFor(n);
  std::size_t step = n >> 1U;
  for (std::size_t cycle = 1; cycle < n; cycle <<= 1U, step >>= 1U) {
    std::size_t k1 = 0;
    std::size_t k2 = step << 1U;
    for (std::size_t c = 0; c < cycle; ++c) {
      const std::size_t nc = t.butter[c] << 1U;
      const float re_phi = t.phi[nc];
      const float im_phi = t.phi[nc + 1];
      for (std::size_t s = 0; s < step; ++s) {
        const float r1 = x[k1];
        const float i1 = x[k1 + 1];
        const float r2 = x[k2];
        const float i2 = x[k2 + 1];
        if (inverse) {
          x[k1] = r1 + re_phi * r2 - im_phi * i2;
          x[k1 + 1] = i1 + im_phi * r2 + re_phi * i2;
          x[k2] = r1 - re_phi * r2 + im_phi * i2;
          x[k2 + 1] = i1 - im_phi * r2 - re_phi * i2;
        } else {
          x[k1] = r1 + re_phi * r2 + im_phi * i2;
          x[k1 + 1] = i1 - im_phi * r2 + re_phi * i2;
          x[k2] = r1 - re_phi * r2 - im_phi * i2;
          x[k2 + 1] = i1 + im_phi * r2 - re_phi * i2;
        }
        k1 += 2;
        k2 += 2;
      }
      k1 = k2;
      k2 = k1 + (step << 1U);
    }
  }
  // The bit-reversal permutation, swapping each pair once.
  const std::size_t half = n >> 1U;
  // Reused per thread: the transform runs for every frame of every
  // signal, and the permutation is rewritten in full each time.
  thread_local std::vector<std::size_t> swap;
  swap.resize(n);
  for (std::size_t c = 0; c < half; ++c) {
    swap[c] = t.butter[c] << 1U;
    swap[c + half] = 1 + swap[c];
  }
  for (std::size_t c = 0; c < n; ++c) {
    const std::size_t s = swap[c];
    if (s != c) {
      swap[s] = s;
      std::swap(x[c << 1U], x[s << 1U]);
      std::swap(x[(c << 1U) + 1], x[(s << 1U) + 1]);
    }
  }
}

}  // namespace

void ComplexFft(std::span<float> x, int n) {
  Transform(x, static_cast<std::size_t>(n), false);
}

void ComplexIfft(std::span<float> x, int n) {
  const auto size = static_cast<std::size_t>(n);
  if (size <= 1) {
    return;
  }
  Transform(x, size, true);
  const auto n_float = static_cast<float>(size);
  for (std::size_t c = 0; c < 2 * size; ++c) {
    x[c] /= n_float;
  }
}

void RealFft(std::span<float> x, int n) {
  const auto size = static_cast<std::size_t>(n);
  assert(x.size() >= size + 2);
  // Reused per thread and written in full below.
  thread_local std::vector<float> y;
  y.resize(2 * size);
  for (std::size_t i = 0; i < size; ++i) {
    y[2 * i] = x[i];
    y[(2 * i) + 1] = 0.0F;
  }
  ComplexFft(y, n);
  for (std::size_t i = 0; i <= size / 2; ++i) {
    x[2 * i] = y[2 * i];
    x[(2 * i) + 1] = y[(2 * i) + 1];
  }
}

void RealIfft(std::span<float> x, int n) {
  const auto size = static_cast<std::size_t>(n);
  assert(x.size() >= size + 2);
  // Reused per thread and written in full below.
  thread_local std::vector<float> y;
  y.resize(2 * size);
  for (std::size_t i = 0; i <= size / 2; ++i) {
    y[2 * i] = x[2 * i];
    y[(2 * i) + 1] = x[(2 * i) + 1];
  }
  for (std::size_t i = (size / 2) + 1; i < size; ++i) {
    const std::size_t j = size - i;
    y[2 * i] = x[2 * j];
    y[(2 * i) + 1] = -x[(2 * j) + 1];
  }
  ComplexIfft(y, n);
  for (std::size_t i = 0; i < size; ++i) {
    x[i] = y[2 * i];
  }
}

int NextPowerOfTwo(int x) {
  int c = 1;
  while (c < x) {
    c <<= 1U;
  }
  return c;
}

std::vector<float> CrossCorrelate(std::span<const float> x1,
                                  std::span<const float> x2) {
  const int n1 = static_cast<int>(x1.size());
  const int n2 = static_cast<int>(x2.size());
  const int nx = NextPowerOfTwo(std::max(n1, n2));
  const std::size_t size = 2 * static_cast<std::size_t>(nx);
  std::vector<float> tmp1(size + 2, 0.0F);
  std::vector<float> tmp2(size + 2, 0.0F);
  // x1 reversed, so the product of the spectra is a correlation.
  for (std::size_t c = 0; c < x1.size(); ++c) {
    tmp1[x1.size() - 1 - c] = x1[c];
  }
  for (std::size_t c = 0; c < x2.size(); ++c) {
    tmp2[c] = x2[c];
  }
  RealFft(tmp1, 2 * nx);
  RealFft(tmp2, 2 * nx);
  for (std::size_t c = 0; std::cmp_less_equal(c, nx); ++c) {
    const std::size_t d = c << 1U;
    const float r1 = tmp1[d];
    const float i1 = tmp1[d + 1];
    tmp1[d] = r1 * tmp2[d] - i1 * tmp2[d + 1];
    tmp1[d + 1] = r1 * tmp2[d + 1] + i1 * tmp2[d];
  }
  RealIfft(tmp1, 2 * nx);
  tmp1.resize(x1.size() + x2.size() - 1);
  return tmp1;
}

}  // namespace speak5220::core::quality
