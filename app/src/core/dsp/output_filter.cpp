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
#include <cmath>
#include <cstddef>
#include <numbers>
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

// The BBC Micro's speech path: the C21/R31 high-pass (1st order, 279 Hz),
// then the IC17 Sallen-Key low-passes (2nd-order Butterworth, 6.95 kHz)
// before and after the mixer. The full circuit differs from this cascade
// by at most 0.1 dB from 30 Hz to 20 kHz (Acorn's sound schematic for the
// Model B, as the research capture board copied it: its audio output stage,
// documented until commit c537c1d).
constexpr double kBbcHighPassHz = 279.0;
constexpr double kBbcLowPassHz = 6950.0;

// A 1st-order high-pass at `cutoff_hz`, by the bilinear transform with the
// cutoff prewarped, as scipy.signal.butter(1, cutoff_hz, "highpass",
// fs=rate, output="sos") gives it.
SosSection FirstOrderHighPass(double cutoff_hz, double rate) {
  const double k = std::tan(std::numbers::pi * cutoff_hz / rate);
  const double norm = 1.0 / (1.0 + k);
  SosSection section;
  section.b = {norm, -norm, 0.0};
  section.a = {(k - 1.0) * norm, 0.0};
  return section;
}

// A 2nd-order Butterworth low-pass at `cutoff_hz`, as
// scipy.signal.butter(2, cutoff_hz, "lowpass", fs=rate, output="sos")
// gives it.
SosSection ButterworthLowPass(double cutoff_hz, double rate) {
  const double k = std::tan(std::numbers::pi * cutoff_hz / rate);
  const double norm = 1.0 / (1.0 + (std::numbers::sqrt2 * k) + (k * k));
  SosSection section;
  section.b = {k * k * norm, 2.0 * k * k * norm, k * k * norm};
  section.a = {2.0 * ((k * k) - 1.0) * norm,
               (1.0 - (std::numbers::sqrt2 * k) + (k * k)) * norm};
  return section;
}

}  // namespace

std::vector<SosSection> OutputFilterSections(OutputFilter filter, double rate) {
  switch (filter) {
    case OutputFilter::kBbcMicro: {
      const SosSection low_pass = ButterworthLowPass(kBbcLowPassHz, rate);
      return {FirstOrderHighPass(kBbcHighPassHz, rate), low_pass, low_pass};
    }
    case OutputFilter::kNone:
      break;
  }
  return {};
}

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
                                   std::span<const float> samples,
                                   int sample_rate) {
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
  const std::vector<SosSection> sections = OutputFilterSections(
      filter, static_cast<double>(sample_rate) * kOutputOversampling);
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
