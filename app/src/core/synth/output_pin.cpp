/*
 * File:        output_pin.cpp
 * Module:      synth
 * Purpose:     The chip's two outputs of the same sound: the SPEAKER pin's
 *              DAC, which clips each code to 8 bits, and the I/O pin's
 *              10-bit codes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/output_pin.h"

#include <algorithm>
#include <array>
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

std::vector<std::int16_t> PinSamples(OutputPin pin,
                                     std::span<const std::int16_t> samples) {
  std::vector<std::int16_t> out(samples.begin(), samples.end());
  if (pin == OutputPin::kSpeaker) {
    for (std::int16_t& v : out) {
      v = std::clamp(v, kLowest, kHighest);
    }
  }
  return out;
}

int ClippedSamples(std::span<const std::int16_t> samples) {
  return static_cast<int>(std::ranges::count_if(
      samples, [](std::int16_t v) { return v < kLowest || v > kHighest; }));
}

}  // namespace speak5220::core::synth
