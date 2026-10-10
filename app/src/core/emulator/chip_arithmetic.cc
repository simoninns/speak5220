/*
 * File:        chip_arithmetic.cc
 * Module:      emulator
 * Purpose:     The TMS5220's bit-exact arithmetic: the Booth multiplier, the
 *              14-bit wrap and the noise register
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/chip_arithmetic.h"

#include <algorithm>
#include <array>
#include <cstddef>

namespace speak5220::core::emulator {
namespace {

// speak5220 datasheet §8.2, Table 8-2: bit weights per load, LSB first.
constexpr std::array<double, 8> kWeights996 = {0.86,  1.82,  3.99,  7.85,
                                               15.79, 32.33, 63.97, 132.64};
constexpr std::array<double, 8> kWeights482 = {0.87,  1.82,  3.94,  7.78,
                                               15.74, 32.09, 62.89, 130.96};
// speak5220 datasheet §8.2, Table 8-1: the LSB current per load.
constexpr double kLsb996 = 4.49;
constexpr double kLsb482 = 4.80;
// The DAC takes the top 8 bits of the code, offset to 0..255.
constexpr int kDacMin = -128;
constexpr int kDacMax = 127;

}  // namespace

double DacCurrentMicroamps(int code, DacLoad load) {
  const std::array<double, 8>& weights =
      load == DacLoad::k482Ohm ? kWeights482 : kWeights996;
  const double lsb = load == DacLoad::k482Ohm ? kLsb482 : kLsb996;
  const int byte = std::clamp(code, kDacMin, kDacMax) - kDacMin;
  double sum = 0.0;
  for (std::size_t i = 0; i < weights.size(); ++i) {
    if (((byte >> i) & 1) == 0) {
      sum += weights.at(i);
    }
  }
  return sum * lsb;
}

}  // namespace speak5220::core::emulator
