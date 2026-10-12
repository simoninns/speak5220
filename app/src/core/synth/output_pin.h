/*
 * File:        output_pin.h
 * Module:      synth
 * Purpose:     The chip's two outputs of the same sound: the SPEAKER pin's
 *              DAC, which clips each code to 8 bits and has the measured
 *              uneven steps, and the I/O pin's 10-bit codes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_SYNTH_OUTPUT_PIN_H_
#define SPEAK5220_CORE_SYNTH_OUTPUT_PIN_H_

#include <cstdint>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace speak5220::core::synth {

// Where the sound is taken from (speak5220 datasheet §8.2 and §8.4).
enum class OutputPin {
  // SPEAKER: the DAC's output, each code clipped to -128..+127 and
  // converted with the DAC's measured bit weights (DacOutput()). How a
  // computer with the chip plays it.
  kSpeaker,
  // I/O: the 10-bit codes, -512..+511, before the DAC clips them. The
  // sound regenerated from the pin's serial output.
  kIo,
};

// What the user sees of a pin.
struct OutputPinInfo {
  OutputPin pin = OutputPin::kSpeaker;
  // A stable name for preferences, such as "speaker".
  std::string_view key;
  // The name in the menu.
  std::string_view label;
};

// Both pins, kSpeaker first, in the order a menu lists them.
std::span<const OutputPinInfo> OutputPins();
const OutputPinInfo& InfoFor(OutputPin pin);
// The pin with `key`, or none for an unknown key.
std::optional<OutputPin> OutputPinFromKey(std::string_view key);

// The SPEAKER pin's output for `code`, clipped to emulator::kDacMin..
// kDacMax first, in steps of the ideal DAC: the input byte's bits (code +
// 128) weighted as the DAC weights them (speak5220 datasheet §8.2, Table
// 8-2), scaled so that -128 and +127 give -128 and +127. A more positive
// value is less current, as the codes are. The steps are uneven: the one
// between -1 and 0, where every bit changes, is about six ideal steps
// (Table 8-3), so -1 gives about -3.5 and 0 about +2.5. Pure; thread-safe.
double DacOutput(int code);

// `samples` (SynthResult::samples, codes x kSampleScale) as `pin` gives
// them: for kSpeaker, each code's DacOutput() x kSampleScale, rounded; for
// kIo, unchanged. O(n). Pure; thread-safe.
std::vector<std::int16_t> PinSamples(OutputPin pin,
                                     std::span<const std::int16_t> samples);

// The number of `samples` outside the DAC's range: those kSpeaker clips.
// O(n). Pure; thread-safe.
int ClippedSamples(std::span<const std::int16_t> samples);

}  // namespace speak5220::core::synth

#endif  // SPEAK5220_CORE_SYNTH_OUTPUT_PIN_H_
