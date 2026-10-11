/*
 * File:        encoder_options.cpp
 * Module:      encoder
 * Purpose:     The encoder's options, with the range, default and help text
 *              the user interface shows for each
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/encoder_options.h"

#include <spdlog/fmt/fmt.h>

#include <array>
#include <cmath>
#include <span>
#include <string_view>
#include <utility>

#include "core/util/status.h"

namespace speak5220::core::encoder {
namespace {

using enum EncoderOptionType;

// speak5220 datasheet §12.6 and the encoder's documentation
// (docs/application/reference/encoder-options.md).
constexpr std::array<EncoderOptionInfo, 15> kInfos = {{
    {.key = "targetRms",
     .label = "Target RMS",
     .help = "RMS of the output over sounding frames, in DAC codes.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 100.0,
     .default_text = "20"},
    {.key = "targetPeak",
     .label = "Target peak",
     .help = "If set, scale so the loudest output peak reaches this many "
             "codes, instead of using the target RMS.",
     .type = kOptionalReal,
     .minimum = 1.0,
     .maximum = 127.0,
     .default_text = ""},
    {.key = "peakLimit",
     .label = "Peak limit",
     .help = "No output code goes beyond this (the DAC's range is +-127).",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 127.0,
     .default_text = "120"},
    {.key = "repeats",
     .label = "Repeat frames",
     .help = "Send a frame whose coefficients barely change as an 11-bit "
             "repeat of the frame before.",
     .type = kBoolean,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "true"},
    {.key = "repeatLambda",
     .label = "Repeat trade-off",
     .help = "Bits a run of repeats may save per dB of extra prediction "
             "error. Lower gives fewer bits.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 1000.0,
     .default_text = "70"},
    {.key = "gateDb",
     .label = "Silence gate",
     .help = "Frames this many dB below the loudest frame become silence.",
     .type = kReal,
     .minimum = 10.0,
     .maximum = 90.0,
     .default_text = "45"},
    {.key = "preEmphasis",
     .label = "Pre-emphasis",
     .help = "Pre-emphasis for the voiced analysis: \"chirp\" whitens the "
             "chip's excitation pulse; a number mu uses 1 - mu z^-1.",
     .type = kPreEmphasis,
     .minimum = 0.0,
     .maximum = 0.99,
     .default_text = "chirp"},
    {.key = "window",
     .label = "Analysis window",
     .help = "Length of the analysis window, in samples at 8 kHz.",
     .type = kInteger,
     .minimum = 80.0,
     .maximum = 480.0,
     .default_text = "240"},
    {.key = "lagWindowHz",
     .label = "Lag window",
     .help = "Bandwidth of the Gaussian lag window, in Hz, which widens the "
             "formant peaks.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 400.0,
     .default_text = "60"},
    {.key = "voicingThreshold",
     .label = "Voicing threshold",
     .help = "Periodicity (CMNDF) level at which voiced and unvoiced cost "
             "the same. Higher gives more voiced frames.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "0.3"},
    {.key = "voicingEvidence",
     .label = "Voicing evidence",
     .help = "Weight of spectral tilt and zero crossings in the voicing "
             "decision; 0 uses periodicity alone.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 20.0,
     .default_text = "4"},
    {.key = "iterations",
     .label = "Level passes",
     .help = "Closed-loop passes that adjust each frame's energy against "
             "the synthesised output.",
     .type = kInteger,
     .minimum = 1.0,
     .maximum = 20.0,
     .default_text = "5"},
    {.key = "fadeUnvoiced",
     .label = "Fade unvoiced endings",
     .help = "Fade hiss out before a silence or the stop.",
     .type = kBoolean,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "true"},
    {.key = "levelAgc",
     .label = "Slow gain control",
     .help = "Compression ratio of a slow (0.75 s) loudness envelope; "
             "brings quiet words up. 1 is off.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 10.0,
     .default_text = "1"},
    {.key = "compression",
     .label = "Compression",
     .help = "Per-frame level compression ratio around the median level. "
             "1 is off.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 10.0,
     .default_text = "1"},
}};

const EncoderOptionInfo& Info(std::string_view key) {
  for (const EncoderOptionInfo& info : kInfos) {
    if (info.key == key) {
      return info;
    }
  }
  return kInfos.front();
}

Status CheckRange(std::string_view key, double value) {
  const EncoderOptionInfo& info = Info(key);
  if (!std::isfinite(value) || value < info.minimum || value > info.maximum) {
    return OutOfRangeError(fmt::format("{} is {}; it must be from {} to {}",
                                       key, value, info.minimum, info.maximum));
  }
  return OkStatus();
}

}  // namespace

std::span<const EncoderOptionInfo> EncoderOptionInfos() { return kInfos; }

Status ValidateEncoderOptions(const EncoderOptions& options) {
  const std::array<std::pair<std::string_view, double>, 11> checks = {{
      {"targetRms", options.target_rms},
      {"peakLimit", options.peak_limit},
      {"repeatLambda", options.repeat_lambda},
      {"gateDb", options.gate_db},
      {"window", static_cast<double>(options.window)},
      {"lagWindowHz", options.lag_window_hz},
      {"voicingThreshold", options.voicing_threshold},
      {"voicingEvidence", options.voicing_evidence},
      {"iterations", static_cast<double>(options.iterations)},
      {"levelAgc", options.level_agc},
      {"compression", options.compression},
  }};
  for (const auto& [key, value] : checks) {
    if (Status status = CheckRange(key, value); !status.IsOk()) {
      return status;
    }
  }
  if (options.target_peak.has_value()) {
    if (Status status = CheckRange("targetPeak", *options.target_peak);
        !status.IsOk()) {
      return status;
    }
  }
  if (!options.pre_emphasis.chirp) {
    if (Status status = CheckRange("preEmphasis", options.pre_emphasis.mu);
        !status.IsOk()) {
      return status;
    }
  }
  return OkStatus();
}

}  // namespace speak5220::core::encoder
