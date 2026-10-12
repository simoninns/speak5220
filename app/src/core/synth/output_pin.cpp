/*
 * File:        output_pin.cpp
 * Module:      synth
 * Purpose:     The chip's two outputs of the same sound: the SPEAKER pin's
 *              DAC, which clips each code to 8 bits and has the measured
 *              uneven steps, and the I/O pin's 10-bit codes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/output_pin.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/emulator/chip_arithmetic.h"
#include "core/synth/synth.h"

namespace speak5220::core::synth {

namespace {

constexpr std::array<OutputPinInfo, 2> kPins = {{
    {.pin = OutputPin::kSpeaker,
     .key = "speaker",
     .label = "SPEAKER (8-bit DAC)"},
    {.pin = OutputPin::kIo, .key = "io", .label = "I/O (10-bit codes)"},
}};

constexpr std::int16_t kLowest = emulator::kDacMin * kSampleScale;
constexpr std::int16_t kHighest = emulator::kDacMax * kSampleScale;

// speak5220 datasheet §8.2, Table 8-2: the weight of each of the DAC's
// input bits, bit 0 first, in LSB, measured into 996 ohms.
constexpr std::array<double, 8> kBitWeights = {0.86,  1.82,  3.99,  7.85,
                                               15.79, 32.33, 63.97, 132.64};
// The input byte is the code plus this (datasheet §8.2: code 0 is 0x80).
constexpr int kInputOffset = 128;
// The ideal DAC's span, from code -128 to +127, in steps.
constexpr double kIdealSpan = 255.0;
// The codes the DAC takes, -128 to +127.
constexpr std::size_t kDacCodes = emulator::kDacMax - emulator::kDacMin + 1;

constexpr double TotalWeight() {
  double total = 0.0;
  for (const double w : kBitWeights) {
    total += w;
  }
  return total;
}

}  // namespace

std::span<const OutputPinInfo> OutputPins() { return kPins; }

const OutputPinInfo& InfoFor(OutputPin pin) {
  for (const OutputPinInfo& info : kPins) {
    if (info.pin == pin) {
      return info;
    }
  }
  return kPins.front();
}

std::optional<OutputPin> OutputPinFromKey(std::string_view key) {
  for (const OutputPinInfo& info : kPins) {
    if (info.key == key) {
      return info.pin;
    }
  }
  return std::nullopt;
}

double DacOutput(int code) {
  const auto byte = static_cast<unsigned>(
      std::clamp(code, emulator::kDacMin, emulator::kDacMax) + kInputOffset);
  double level = 0.0;
  for (std::size_t bit = 0; bit < kBitWeights.size(); ++bit) {
    if ((byte >> bit) & 1U) {
      level += kBitWeights[bit];
    }
  }
  return (level * kIdealSpan / TotalWeight()) - kInputOffset;
}

std::vector<std::int16_t> PinSamples(OutputPin pin,
                                     std::span<const std::int16_t> samples) {
  std::vector<std::int16_t> out(samples.begin(), samples.end());
  if (pin == OutputPin::kSpeaker) {
    // One level per code, worked out once.
    static const std::array<std::int16_t, kDacCodes> kLevels = [] {
      std::array<std::int16_t, kDacCodes> levels{};
      for (std::size_t i = 0; i < levels.size(); ++i) {
        levels[i] = static_cast<std::int16_t>(std::lround(
            DacOutput(static_cast<int>(i) + emulator::kDacMin) * kSampleScale));
      }
      return levels;
    }();
    for (std::int16_t& v : out) {
      const int code = std::clamp(v, kLowest, kHighest) / kSampleScale;
      v = kLevels[static_cast<std::size_t>(code - emulator::kDacMin)];
    }
  }
  return out;
}

int ClippedSamples(std::span<const std::int16_t> samples) {
  return static_cast<int>(std::ranges::count_if(
      samples, [](std::int16_t v) { return v < kLowest || v > kHighest; }));
}

}  // namespace speak5220::core::synth
