/*
 * File:        output_filter.h
 * Module:      dsp
 * Purpose:     Models the analogue audio stage a computer puts after the
 *              TMS5220's DAC, for listening to and viewing the chip's
 *              output as that computer would play it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_OUTPUT_FILTER_H_
#define SPEAK5220_CORE_DSP_OUTPUT_FILTER_H_

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/dsp/iir_filter.h"

namespace speak5220::core::dsp {

// The audio stage after the DAC.
enum class OutputFilter {
  // None: the DAC's samples as they are.
  kNone,
  // The BBC Microcomputer Model B's speech path: a 279 Hz 1st-order
  // high-pass and two 2nd-order Butterworth low-passes at 6.95 kHz
  // (docs/technical/hardware/audio-output-stage.md).
  kBbcMicro,
};

// What the user sees of a filter.
struct OutputFilterInfo {
  OutputFilter filter = OutputFilter::kNone;
  // A stable name for preferences, such as "bbc-micro".
  std::string_view key;
  // The name in the menu, such as "BBC Micro".
  std::string_view label;
};

// Every filter, kNone first, in the order a menu lists them.
std::span<const OutputFilterInfo> OutputFilters();
const OutputFilterInfo& InfoFor(OutputFilter filter);
// The filter with `key`, or none for an unknown key.
std::optional<OutputFilter> OutputFilterFromKey(std::string_view key);

// The chip's sample rate at its nominal clock (speak5220 datasheet §2).
inline constexpr int kChipSampleRate = 8000;
// The filters run at this multiple of the chip's sample rate, so that the
// DAC's staircase and its images above half that rate are modelled.
inline constexpr int kOutputOversampling = 6;
// The filters' rate at the nominal clock: 48 kHz.
inline constexpr int kOutputRate = kChipSampleRate * kOutputOversampling;

// `filter` as second-order sections at `rate` (Hz), the analogue stage's
// corners at the same frequencies whatever the rate; none for kNone.
// Pure; thread-safe.
std::vector<SosSection> OutputFilterSections(OutputFilter filter, double rate);

// The output of a DAC that holds each of `samples` (at `sample_rate`,
// full scale +-1) for one sample period, through `filter`, at
// `sample_rate` x kOutputOversampling: kOutputOversampling values per
// sample. A chip clocked off its nominal rate gives its samples at another
// rate (core::synth::ChipSampleRate()); the analogue stage after it stays
// the same. The filters' passband gain is 1,
// so the level stays as it was; the analogue stage's own gain is left
// out. kNone gives the held samples alone. The filters start at rest with
// the first sample taken as zero, so a constant start has no transient.
// O(n). Pure; thread-safe.
std::vector<float> FilterDacOutput(OutputFilter filter,
                                   std::span<const float> samples,
                                   int sample_rate = kChipSampleRate);

// FilterDacOutput() with one value per 8 kHz sample, the mean over its
// period, for a waveform drawn at 8 kHz. kNone returns `samples`
// unchanged. O(n). Pure; thread-safe.
std::vector<float> FilterSamples(OutputFilter filter,
                                 std::span<const float> samples);

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_OUTPUT_FILTER_H_
