/*
 * File:        numpy_normal.cpp
 * Module:      quality
 * Purpose:     NumPy's default generator (PCG64) and its ziggurat
 *              standard normal sampler, reproduced bit for bit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/numpy_normal.h"

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "core/quality/normal_ziggurat_tables.h"

namespace speak5220::core::quality {

namespace {

namespace z = normal_ziggurat;

// PCG's 128-bit LCG multiplier (pcg64.h PCG_DEFAULT_MULTIPLIER_128).
constexpr Uint128 kMultiplier = {.high = 2549297995355413924ULL,
                                 .low = 4865540595714422341ULL};

// The state numpy.random.PCG64(1) holds after seeding through
// numpy.random.SeedSequence(1) (NumPy 2.4.4).
constexpr Uint128 kSeedOneState = {.high = 0x9c5b484bfedb756cULL,
                                   .low = 0x2a6e7d6f320fbc7eULL};
constexpr Uint128 kSeedOneIncrement = {.high = 0x922af2da2645f895ULL,
                                       .low = 0xa19857b95740937bULL};

// 2^-53: next_double()'s scale.
constexpr double kDoubleUnit = 1.0 / 9007199254740992.0;

constexpr std::uint64_t kLow32 = 0xffffffffULL;

// The full 128-bit product of two 64-bit values.
Uint128 Multiply64(std::uint64_t a, std::uint64_t b) {
  const std::uint64_t a_lo = a & kLow32;
  const std::uint64_t a_hi = a >> 32U;
  const std::uint64_t b_lo = b & kLow32;
  const std::uint64_t b_hi = b >> 32U;
  const std::uint64_t lo_lo = a_lo * b_lo;
  const std::uint64_t hi_lo = a_hi * b_lo;
  const std::uint64_t lo_hi = a_lo * b_hi;
  const std::uint64_t hi_hi = a_hi * b_hi;
  const std::uint64_t cross = (lo_lo >> 32U) + (hi_lo & kLow32) + lo_hi;
  return {.high = hi_hi + (hi_lo >> 32U) + (cross >> 32U),
          .low = (cross << 32U) | (lo_lo & kLow32)};
}

// a * b + c, modulo 2^128.
Uint128 MultiplyAdd(const Uint128& a, const Uint128& b, const Uint128& c) {
  Uint128 p = Multiply64(a.low, b.low);
  p.high += (a.high * b.low) + (a.low * b.high);
  const std::uint64_t low = p.low + c.low;
  const std::uint64_t carry = low < p.low ? 1 : 0;
  return {.high = p.high + c.high + carry, .low = low};
}

std::uint64_t RotateRight(std::uint64_t value, unsigned rot) {
  return (value >> rot) | (value << ((64U - rot) & 63U));
}

}  // namespace

NumpyNormal::NumpyNormal(Uint128 state, Uint128 increment)
    : state_(state), increment_(increment) {}

NumpyNormal NumpyNormal::SeedOne() {
  return {kSeedOneState, kSeedOneIncrement};
}

std::uint64_t NumpyNormal::NextUint64() {
  // pcg_setseq_128_xsl_rr_64_random_r(): step, then output the new state.
  state_ = MultiplyAdd(state_, kMultiplier, increment_);
  return RotateRight(state_.high ^ state_.low,
                     static_cast<unsigned>(state_.high >> 58U));
}

double NumpyNormal::NextDouble() {
  return static_cast<double>(NextUint64() >> 11U) * kDoubleUnit;
}

double NumpyNormal::Next() {
  // random_standard_normal(): the low 8 bits pick a layer, the next its
  // sign, and 52 bits its magnitude; most draws fall inside their layer
  // and are returned at once.
  constexpr std::uint64_t kMagnitudeMask = 0x000fffffffffffffULL;
  while (true) {
    std::uint64_t r = NextUint64();
    const auto idx = static_cast<std::size_t>(r & 0xffU);
    r >>= 8U;
    const bool negative = (r & 0x1U) != 0;
    const std::uint64_t rabs = (r >> 1U) & kMagnitudeMask;
    double x = static_cast<double>(rabs) * z::kWi[idx];
    if (negative) {
      x = -x;
    }
    if (rabs < z::kKi[idx]) {
      return x;
    }
    if (idx == 0) {
      // The tail, by Marsaglia's method.
      while (true) {
        const double xx = -z::kInverseTailStart * std::log1p(-NextDouble());
        const double yy = -std::log1p(-NextDouble());
        if (yy + yy > xx * xx) {
          return ((rabs >> 8U) & 0x1U) != 0 ? -(z::kTailStart + xx)
                                            : z::kTailStart + xx;
        }
      }
    } else if (((z::kFi[idx - 1] - z::kFi[idx]) * NextDouble() + z::kFi[idx]) <
               std::exp(-0.5 * x * x)) {
      return x;
    }
  }
}

}  // namespace speak5220::core::quality
