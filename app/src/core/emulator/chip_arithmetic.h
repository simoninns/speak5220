/*
 * File:        chip_arithmetic.h
 * Module:      emulator
 * Purpose:     The TMS5220's bit-exact arithmetic: the Booth multiplier, the
 *              14-bit wrap and the noise register
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_CHIP_ARITHMETIC_H_
#define SPEAK5220_CORE_EMULATOR_CHIP_ARITHMETIC_H_

#include <array>
#include <cstdint>

namespace speak5220::core::emulator {

// The noise register's width and its state after Reset (§7, §8.6).
inline constexpr std::uint32_t kNoiseMask = 0x1FFF;
inline constexpr std::uint32_t kNoiseResetState = 0x1FFF;
// The lattice's sums are 14-bit two's complement (§8.8).
inline constexpr std::int32_t kLatticeLimit = 8192;

// Arithmetic shift right, rounding down. C++20 defines >> on negative
// values this way. Re-implements tms5220.c tms5220_asr().
constexpr std::int32_t Asr(std::int32_t x, int n) { return x >> n; }

// speak5220 datasheet §8.8: two's-complement wrap to 14 bits,
// -8192..8191. Re-implements tms5220.c tms5220_wrap14().
constexpr std::int32_t Wrap14(std::int32_t x) {
  const std::uint32_t wrapped =
      (static_cast<std::uint32_t>(x) + 8192U) & 0x3FFFU;
  return static_cast<std::int32_t>(wrapped) - kLatticeLimit;
}

// speak5220 datasheet §8.8: the radix-4 Booth digits d0..d4 of a 10-bit
// two's-complement coefficient, d_i = -2*k(2i+1) + k(2i) + k(2i-1).
// Re-implements tms5220enc.py boothDigits().
constexpr std::array<int, 5> BoothDigits(int k) {
  const auto bits = static_cast<unsigned>(k) & 0x3FFU;
  std::array<int, 5> digits = {};
  int previous = 0;
  for (unsigned i = 0; i < 5; ++i) {
    const auto low = static_cast<int>((bits >> (2 * i)) & 1U);
    const auto high = static_cast<int>((bits >> (2 * i + 1)) & 1U);
    digits.at(i) = -2 * high + low + previous;
    previous = high;
  }
  return digits;
}

// speak5220 datasheet §8.8: the array multiplier, booth(K, M) of §8.9 with
// the operands in the order (M, K). m is the 14-bit operand, k the 10-bit
// coefficient. Partial product 0 is inverted without the +1 for a negative
// d0, partial products 1-4 are exact, nothing wraps inside, and P0 is
// always made a logical one. Re-implements tms5220.c tms5220_booth().
constexpr std::int32_t Booth(std::int32_t m, int k) {
  const std::array<int, 5> d = BoothDigits(k);
  std::int32_t acc = d[0] < 0 ? -((-d[0]) * m) - 1 : d[0] * m;
  acc = Asr(acc, 2) + d[1] * m;
  for (std::size_t i = 2; i < d.size(); ++i) {
    acc = Asr(acc, 2) + d.at(i) * m;
  }
  return Asr(acc, 1) | 1;
}

// The same product in closed form, floor((K*M - k1) / 512) OR 1, where k1
// is bit 1 of K: the nested shifts collapse to one floor division because
// the digits sum to K. Re-implements tms5220enc.py boothFast().
constexpr std::int32_t BoothClosedForm(std::int32_t m, int k) {
  return ((k * m - ((k >> 1) & 1)) >> 9) | 1;
}

// speak5220 datasheet §8.6: one step of the 13-bit noise register; shift
// left, new bit 0 = b12 ^ b3 ^ b2 ^ b0. Re-implements tms5220.c
// tms5220_noise_step().
constexpr std::uint32_t NoiseStep(std::uint32_t n) {
  const std::uint32_t feedback = ((n >> 12) ^ (n >> 3) ^ (n >> 2) ^ n) & 1U;
  return ((n << 1) | feedback) & kNoiseMask;
}

// speak5220 datasheet §8.2: the DAC takes the I/O code clipped to this
// range, offset to 0..255; I/O itself carries the code unclipped.
inline constexpr int kDacMin = -128;
inline constexpr int kDacMax = 127;

// The SPEAKER load of §8.2, Table 8-2.
enum class DacLoad { k996Ohm, k482Ohm };

// speak5220 datasheet §8.2: the approximate SPEAKER current in microamps
// for an I/O code, from the Table 8-2 bit weights and the Table 8-1 LSB.
// Re-implements tms5220.c tms5220_dac_current_ua().
double DacCurrentMicroamps(int code, DacLoad load);

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_CHIP_ARITHMETIC_H_
