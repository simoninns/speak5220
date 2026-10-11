/*
 * File:        output_filter.cpp
 * Module:      dsp
 * Purpose:     Models the analogue audio stage a computer puts after the
 *              TMS5220's DAC, for listening to and viewing the chip's
 *              output as that computer would play it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/output_filter.h"

#include <array>
#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/dsp/iir_filter.h"

namespace speak5220::core::dsp {

namespace {

constexpr std::array<OutputFilterInfo, 2> kFilters = {{
    {.filter = OutputFilter::kNone, .key = "none", .label = "None"},
    {.filter = OutputFilter::kBbcMicro,
     .key = "bbc-micro",
     .label = "BBC Micro"},
}};

// The BBC Micro's speech path at kOutputRate, as
// scipy.signal.butter(order, hz, kind, fs=48000, output="sos") gives it:
// the C21/R31 high-pass (1st order, 279 Hz), then the IC17 Sallen-Key
// low-passes (2nd-order Butterworth, 6.95 kHz) before and after the
// mixer. The full circuit differs from this cascade by at most 0.1 dB from
// 30 Hz to 20 kHz (docs/technical/hardware/audio-output-stage.md, "Overall
// response"); investigation/tools/process.py listenAudio() uses the same.
constexpr SosSection kBbcHighPass = {
    .b = {0.9820650013779968, -0.9820650013779968, 0.0},
    .a = {-0.9641300027559936, 0.0}};
constexpr SosSection kBbcLowPass = {
    .b = {0.12388333267370166, 0.24776666534740333, 0.12388333267370166},
    .a = {-0.7880350454339105, 0.2835683761287171}};
constexpr std::array<SosSection, 3> kBbcSections = {kBbcHighPass, kBbcLowPass,
                                                    kBbcLowPass};

std::span<const SosSection> SectionsFor(OutputFilter filter) {
  switch (filter) {
    case OutputFilter::kBbcMicro:
      return kBbcSections;
    case OutputFilter::kNone:
      break;
  }
  return {};
}

}  // namespace

std::span<const OutputFilterInfo> OutputFilters() { return kFilters; }

const OutputFilterInfo& InfoFor(OutputFilter filter) {
  for (const OutputFilterInfo& info : kFilters) {
    if (info.filter == filter) {
      return info;
    }
  }
  return kFilters.front();
}

std::optional<OutputFilter> OutputFilterFromKey(std::string_view key) {
  for (const OutputFilterInfo& info : kFilters) {
    if (info.key == key) {
      return info.filter;
    }
  }
  return std::nullopt;
}

std::vector<float> FilterDacOutput(OutputFilter filter,
                                   std::span<const float> samples) {
  if (samples.empty()) {
    return {};
  }
  // The DAC holds each sample for its whole period (speak5220 datasheet
  // §8.2), measured from the first so the filters start settled.
  const double first = samples.front();
  std::vector<double> held;
  held.reserve(samples.size() * kOutputOversampling);
  for (const float v : samples) {
    held.insert(held.end(), kOutputOversampling, v - first);
  }
  const std::span<const SosSection> sections = SectionsFor(filter);
  if (sections.empty()) {
    // Nothing to settle: the samples as they were.
    for (double& v : held) {
      v += first;
    }
  } else {
    std::vector<double> state(sections.size() * 2, 0.0);
    SosFilterInPlace(sections, state, held);
  }
  return {held.begin(), held.end()};
}

std::vector<float> FilterSamples(OutputFilter filter,
                                 std::span<const float> samples) {
  if (filter == OutputFilter::kNone) {
    return {samples.begin(), samples.end()};
  }
  const std::vector<float> output = FilterDacOutput(filter, samples);
  std::vector<float> out;
  out.reserve(samples.size());
  for (std::size_t i = 0; i < output.size(); i += kOutputOversampling) {
    double sum = 0.0;
    for (std::size_t k = 0; k < kOutputOversampling; ++k) {
      sum += output[i + k];
    }
    out.push_back(static_cast<float>(sum / kOutputOversampling));
  }
  return out;
}

}  // namespace speak5220::core::dsp
