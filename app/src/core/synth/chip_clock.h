/*
 * File:        chip_clock.h
 * Module:      synth
 * Purpose:     The oscillator rates the chip's output can be heard at: the
 *              nominal 640 kHz and a range either side, as an RC oscillator
 *              trimmed high or low gives
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_SYNTH_CHIP_CLOCK_H_
#define SPEAK5220_CORE_SYNTH_CHIP_CLOCK_H_

#include <optional>
#include <span>
#include <string_view>

namespace speak5220::core::synth {

// speak5220 datasheet §2: the oscillator runs at 640 kHz for an 8 kHz
// sample rate.
inline constexpr int kNominalClockHz = 640000;
// speak5220 datasheet §2: one sample period is 80 oscillator cycles (20
// slots of four).
inline constexpr int kClocksPerSample = 80;

// One clock rate the user can choose.
struct ChipClockInfo {
  int hz = kNominalClockHz;
  // The name in the menu, such as "660 kHz (+3.1%)".
  std::string_view label;
};

// Every rate, slowest first, in 20 kHz steps from 560 kHz to 720 kHz
// (-12.5% to +12.5%), kNominalClockHz among them. Every rate is a whole
// number of samples per second.
std::span<const ChipClockInfo> ChipClocks();
// `hz` if it is one of ChipClocks(), or none.
std::optional<int> ChipClockFromHz(int hz);

// The sample rate the chip gives at `clock_hz`: clock_hz /
// kClocksPerSample, 8000 at the nominal clock. A faster clock raises the
// pitch and shortens every frame alike (datasheet §2). Pure.
constexpr int ChipSampleRate(int clock_hz) {
  return clock_hz / kClocksPerSample;
}

}  // namespace speak5220::core::synth

#endif  // SPEAK5220_CORE_SYNTH_CHIP_CLOCK_H_
