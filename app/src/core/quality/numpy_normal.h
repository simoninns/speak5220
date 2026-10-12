/*
 * File:        numpy_normal.h
 * Module:      quality
 * Purpose:     NumPy's default generator (PCG64) and its ziggurat
 *              standard normal sampler, reproduced bit for bit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_NUMPY_NORMAL_H_
#define SPEAK5220_CORE_QUALITY_NUMPY_NORMAL_H_

#include <cstdint>

namespace speak5220::core::quality {

// An unsigned 128-bit integer as two 64-bit halves (MSVC has no
// __int128).
struct Uint128 {
  std::uint64_t high = 0;
  std::uint64_t low = 0;
  friend bool operator==(const Uint128&, const Uint128&) = default;
};

// The anchor vocoder's noise (plan §3.3) is the reference's sample for
// sample, so the anchor's output is the reference tools' exactly. This is
// numpy.random.Generator's standard_normal() over PCG64 (XSL-RR 128/64), as in
// NumPy 2.4.4's pcg64.h and distributions.c. Not thread-safe; use one generator
// per thread.
class NumpyNormal {
 public:
  // A generator in the given PCG64 state.
  NumpyNormal(Uint128 state, Uint128 increment);

  // The generator numpy.random.default_rng(1) starts with.
  static NumpyNormal SeedOne();

  // The next standard normal value (random_standard_normal()).
  double Next();

  // The next raw 64-bit output (pcg64_next64()).
  std::uint64_t NextUint64();

  // The next uniform value in [0, 1), 53 bits (next_double()).
  double NextDouble();

 private:
  Uint128 state_;
  Uint128 increment_;
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_NUMPY_NORMAL_H_
