/*
 * File:        chip_clock.cpp
 * Module:      synth
 * Purpose:     The oscillator rates the chip's output can be heard at: the
 *              nominal 640 kHz and a range either side, as an RC oscillator
 *              trimmed high or low gives
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/chip_clock.h"

#include <array>
#include <optional>
#include <span>

namespace speak5220::core::synth {

namespace {

constexpr std::array<ChipClockInfo, 9> kClocks = {{
    {.hz = 560000, .label = "560 kHz (-12.5%)"},
    {.hz = 580000, .label = "580 kHz (-9.4%)"},
    {.hz = 600000, .label = "600 kHz (-6.3%)"},
    {.hz = 620000, .label = "620 kHz (-3.1%)"},
    {.hz = kNominalClockHz, .label = "640 kHz (nominal)"},
    {.hz = 660000, .label = "660 kHz (+3.1%)"},
    {.hz = 680000, .label = "680 kHz (+6.3%)"},
    {.hz = 700000, .label = "700 kHz (+9.4%)"},
    {.hz = 720000, .label = "720 kHz (+12.5%)"},
}};

}  // namespace

std::span<const ChipClockInfo> ChipClocks() { return kClocks; }

std::optional<int> ChipClockFromHz(int hz) {
  for (const ChipClockInfo& info : kClocks) {
    if (info.hz == hz) {
      return hz;
    }
  }
  return std::nullopt;
}

}  // namespace speak5220::core::synth
