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
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/util/number_text.h"
#include "core/util/status.h"

namespace speak5220::core::encoder {
namespace {

using enum EncoderOptionType;

// speak5220 datasheet §12.6 and the encoder's documentation
// (docs/application/reference/encoder-options.md).
constexpr std::array<EncoderOptionInfo, 17> kInfos = {{
    {.key = "targetRms",
     .field = "target_rms",
     .label = "Target RMS",
     .help = "RMS of the output over sounding frames, in DAC codes.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 100.0,
     .default_text = "20",
     .unit = "codes",
     .typical_minimum = 5.0,
     .typical_maximum = 50.0,
     .step = 0.5},
    {.key = "targetPeak",
     .field = "target_peak",
     .label = "Target peak",
     .help = "If set, scale so the loudest output peak reaches this many "
             "codes, instead of using the target RMS.",
     .type = kOptionalReal,
     .minimum = 1.0,
     .maximum = 127.0,
     .default_text = "",
     .unit = "codes",
     .typical_minimum = 30.0,
     .typical_maximum = 127.0,
     .step = 1.0},
    {.key = "peakLimit",
     .field = "peak_limit",
     .label = "Peak limit",
     .help = "No output code goes beyond this (the DAC's range is +-127).",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 127.0,
     .default_text = "120",
     .unit = "codes",
     .typical_minimum = 60.0,
     .typical_maximum = 127.0,
     .step = 1.0},
    {.key = "repeats",
     .field = "repeats",
     .label = "Repeat frames",
     .help = "Send a frame whose coefficients barely change as an 11-bit "
             "repeat of the frame before.",
     .type = kBoolean,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "true",
     .unit = "",
     .typical_minimum = 0.0,
     .typical_maximum = 1.0,
     .step = 1.0},
    {.key = "repeatLambda",
     .field = "repeat_lambda",
     .label = "Repeat trade-off",
     .help = "Bits a run of repeats may save per dB of extra prediction "
             "error. Lower gives fewer bits.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 1000.0,
     .default_text = "70",
     .unit = "bits/dB",
     .typical_minimum = 0.0,
     .typical_maximum = 300.0,
     .step = 5.0},
    {.key = "gateDb",
     .field = "gate_db",
     .label = "Silence gate",
     .help = "Frames this many dB below the loudest frame become silence.",
     .type = kReal,
     .minimum = 10.0,
     .maximum = 90.0,
     .default_text = "45",
     .unit = "dB",
     .typical_minimum = 20.0,
     .typical_maximum = 70.0,
     .step = 0.5},
    {.key = "squelchDb",
     .field = "squelch_db",
     .label = "Squelch",
     .help = "If set, stretches of the recording this many dB below its "
             "loudest sound, such as breaths and room noise, are made "
             "silent before encoding, and the quality is measured against "
             "the squelched recording.",
     .type = kOptionalReal,
     .minimum = 6.0,
     .maximum = 80.0,
     .default_text = "",
     .unit = "dB",
     .typical_minimum = 12.0,
     .typical_maximum = 50.0,
     .step = 1.0},
    {.key = "preEmphasis",
     .field = "pre_emphasis",
     .label = "Pre-emphasis",
     .help = "Pre-emphasis for the voiced analysis: \"chirp\" whitens the "
             "chip's excitation pulse; a number mu uses 1 - mu z^-1.",
     .type = kPreEmphasis,
     .minimum = 0.0,
     .maximum = 0.99,
     .default_text = "chirp",
     .unit = "",
     .typical_minimum = 0.5,
     .typical_maximum = 0.99,
     .step = 0.01},
    {.key = "window",
     .field = "window",
     .label = "Analysis window",
     .help = "Length of the analysis window, in samples at 8 kHz.",
     .type = kInteger,
     .minimum = 80.0,
     .maximum = 480.0,
     .default_text = "240",
     .unit = "samples",
     .typical_minimum = 120.0,
     .typical_maximum = 400.0,
     .step = 8.0},
    {.key = "lagWindowHz",
     .field = "lag_window_hz",
     .label = "Lag window",
     .help = "Bandwidth of the Gaussian lag window, in Hz, which widens the "
             "formant peaks.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 400.0,
     .default_text = "60",
     .unit = "Hz",
     .typical_minimum = 0.0,
     .typical_maximum = 200.0,
     .step = 5.0},
    {.key = "voicingThreshold",
     .field = "voicing_threshold",
     .label = "Voicing threshold",
     .help = "Periodicity (CMNDF) level at which voiced and unvoiced cost "
             "the same. Higher gives more voiced frames.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "0.3",
     .unit = "",
     .typical_minimum = 0.05,
     .typical_maximum = 0.7,
     .step = 0.01},
    {.key = "voicingEvidence",
     .field = "voicing_evidence",
     .label = "Voicing evidence",
     .help = "Weight of spectral tilt and zero crossings in the voicing "
             "decision; 0 uses periodicity alone.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 20.0,
     .default_text = "4",
     .unit = "",
     .typical_minimum = 0.0,
     .typical_maximum = 12.0,
     .step = 0.25},
    {.key = "iterations",
     .field = "iterations",
     .label = "Level passes",
     .help = "Closed-loop passes that adjust each frame's energy against "
             "the synthesised output.",
     .type = kInteger,
     .minimum = 1.0,
     .maximum = 20.0,
     .default_text = "5",
     .unit = "passes",
     .typical_minimum = 1.0,
     .typical_maximum = 10.0,
     .step = 1.0},
    {.key = "fadeUnvoiced",
     .field = "fade_unvoiced",
     .label = "Fade unvoiced endings",
     .help = "Fade hiss out before a silence or the stop.",
     .type = kBoolean,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "true",
     .unit = "",
     .typical_minimum = 0.0,
     .typical_maximum = 1.0,
     .step = 1.0},
    {.key = "quietPauses",
     .field = "quiet_pauses",
     .label = "Quiet pauses",
     .help = "Start each pause of two or more silent frames with one "
             "quiet frame that settles the chip's filter. Without it the "
             "chip buzzes faintly through the pause.",
     .type = kBoolean,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "true",
     .unit = "",
     .typical_minimum = 0.0,
     .typical_maximum = 1.0,
     .step = 1.0},
    {.key = "levelAgc",
     .field = "level_agc",
     .label = "Slow gain control",
     .help = "Compression ratio of a slow (0.75 s) loudness envelope; "
             "brings quiet words up. 1 is off.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 10.0,
     .default_text = "1",
     .unit = ":1",
     .typical_minimum = 1.0,
     .typical_maximum = 6.0,
     .step = 0.1},
    {.key = "compression",
     .field = "compression",
     .label = "Compression",
     .help = "Per-frame level compression ratio around the median level. "
             "1 is off.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 10.0,
     .default_text = "1",
     .unit = ":1",
     .typical_minimum = 1.0,
     .typical_maximum = 6.0,
     .step = 0.1},
}};

// The fields of each type, by name, for SetEncoderOption() and
// EncoderOptionText(). pre_emphasis has a type of its own.
struct RealField {
  std::string_view field;
  double EncoderOptions::* member;
};
struct IntegerField {
  std::string_view field;
  int EncoderOptions::* member;
};
struct BooleanField {
  std::string_view field;
  bool EncoderOptions::* member;
};
struct OptionalRealField {
  std::string_view field;
  std::optional<double> EncoderOptions::* member;
};

constexpr std::array<RealField, 9> kRealFields = {{
    {.field = "target_rms", .member = &EncoderOptions::target_rms},
    {.field = "peak_limit", .member = &EncoderOptions::peak_limit},
    {.field = "repeat_lambda", .member = &EncoderOptions::repeat_lambda},
    {.field = "gate_db", .member = &EncoderOptions::gate_db},
    {.field = "lag_window_hz", .member = &EncoderOptions::lag_window_hz},
    {.field = "voicing_threshold",
     .member = &EncoderOptions::voicing_threshold},
    {.field = "voicing_evidence", .member = &EncoderOptions::voicing_evidence},
    {.field = "level_agc", .member = &EncoderOptions::level_agc},
    {.field = "compression", .member = &EncoderOptions::compression},
}};
constexpr std::array<IntegerField, 2> kIntegerFields = {{
    {.field = "window", .member = &EncoderOptions::window},
    {.field = "iterations", .member = &EncoderOptions::iterations},
}};
constexpr std::array<BooleanField, 3> kBooleanFields = {{
    {.field = "repeats", .member = &EncoderOptions::repeats},
    {.field = "fade_unvoiced", .member = &EncoderOptions::fade_unvoiced},
    {.field = "quiet_pauses", .member = &EncoderOptions::quiet_pauses},
}};
constexpr std::array<OptionalRealField, 2> kOptionalRealFields = {{
    {.field = "target_peak", .member = &EncoderOptions::target_peak},
    {.field = "squelch_db", .member = &EncoderOptions::squelch_db},
}};

template <typename Table>
auto MemberOf(const Table& table, std::string_view field)
    -> decltype(table.front().member) {
  for (const auto& entry : table) {
    if (entry.field == field) {
      return entry.member;
    }
  }
  return nullptr;
}

Status CheckRange(std::string_view field, double value) {
  const EncoderOptionInfo* info = FindEncoderOption(field);
  if (info == nullptr) {
    return InternalError(fmt::format("no range for option {}", field));
  }
  if (!std::isfinite(value) || value < info->minimum || value > info->maximum) {
    return OutOfRangeError(fmt::format("{} is {}; it must be from {} to {}",
                                       field, value, info->minimum,
                                       info->maximum));
  }
  return OkStatus();
}

Status WrongType(std::string_view field, std::string_view expected,
                 std::string_view text) {
  return InvalidArgumentError(
      fmt::format("{} must be {}, not \"{}\"", field, expected, text));
}

}  // namespace

std::span<const EncoderOptionInfo> EncoderOptionInfos() { return kInfos; }

const EncoderOptionInfo* FindEncoderOption(std::string_view field) {
  for (const EncoderOptionInfo& info : kInfos) {
    if (info.field == field) {
      return &info;
    }
  }
  return nullptr;
}

Status ValidateEncoderOptions(const EncoderOptions& options) {
  for (const RealField& entry : kRealFields) {
    if (Status status = CheckRange(entry.field, options.*entry.member);
        !status.IsOk()) {
      return status;
    }
  }
  for (const IntegerField& entry : kIntegerFields) {
    if (Status status =
            CheckRange(entry.field, static_cast<double>(options.*entry.member));
        !status.IsOk()) {
      return status;
    }
  }
  for (const OptionalRealField& entry : kOptionalRealFields) {
    if (const std::optional<double>& value = options.*entry.member;
        value.has_value()) {
      if (Status status = CheckRange(entry.field, *value); !status.IsOk()) {
        return status;
      }
    }
  }
  if (!options.pre_emphasis.chirp) {
    if (Status status = CheckRange("pre_emphasis", options.pre_emphasis.mu);
        !status.IsOk()) {
      return status;
    }
  }
  return OkStatus();
}

bool IsLeftOutAtDefault(const EncoderOptions& options, std::string_view field) {
  return (field == "squelch_db" && !options.squelch_db.has_value()) ||
         (field == "quiet_pauses" && options.quiet_pauses);
}

Status SetEncoderOption(EncoderOptions& options, std::string_view field,
                        std::string_view text) {
  const EncoderOptionInfo* info = FindEncoderOption(field);
  if (info == nullptr) {
    return InvalidArgumentError(
        fmt::format("unknown encoder option \"{}\"", field));
  }
  EncoderOptions updated = options;
  switch (info->type) {
    case kReal: {
      const std::optional<double> value = ParseNumber(text);
      if (!value) {
        return WrongType(field, "a number", text);
      }
      updated.*MemberOf(kRealFields, field) = *value;
      break;
    }
    case kInteger: {
      const std::optional<double> value = ParseNumber(text);
      if (!value || std::trunc(*value) != *value) {
        return WrongType(field, "a whole number", text);
      }
      if (Status status = CheckRange(field, *value); !status.IsOk()) {
        return status;
      }
      updated.*MemberOf(kIntegerFields, field) = static_cast<int>(*value);
      break;
    }
    case kBoolean:
      if (text != "true" && text != "false") {
        return WrongType(field, "true or false", text);
      }
      updated.*MemberOf(kBooleanFields, field) = text == "true";
      break;
    case kOptionalReal:
      if (text.empty() || text == kUnsetOptionText) {
        (updated.*MemberOf(kOptionalRealFields, field)).reset();
      } else if (const std::optional<double> value = ParseNumber(text)) {
        updated.*MemberOf(kOptionalRealFields, field) = value;
      } else {
        return WrongType(field, "a number or none", text);
      }
      break;
    case kPreEmphasis:
      if (text == "chirp") {
        updated.pre_emphasis = PreEmphasis::Chirp();
      } else if (const std::optional<double> value = ParseNumber(text)) {
        updated.pre_emphasis = PreEmphasis::FirstOrder(*value);
      } else {
        return WrongType(field, "chirp or a number", text);
      }
      break;
  }
  if (Status status = ValidateEncoderOptions(updated); !status.IsOk()) {
    return status;
  }
  options = updated;
  return OkStatus();
}

std::string EncoderOptionText(const EncoderOptions& options,
                              std::string_view field) {
  const EncoderOptionInfo* info = FindEncoderOption(field);
  if (info == nullptr) {
    return {};
  }
  switch (info->type) {
    case kReal:
      return FormatNumber(options.*MemberOf(kRealFields, field));
    case kInteger:
      return std::to_string(options.*MemberOf(kIntegerFields, field));
    case kBoolean:
      return options.*MemberOf(kBooleanFields, field) ? "true" : "false";
    case kOptionalReal: {
      const std::optional<double>& value =
          options.*MemberOf(kOptionalRealFields, field);
      return value.has_value() ? FormatNumber(*value)
                               : std::string(kUnsetOptionText);
    }
    case kPreEmphasis:
      return options.pre_emphasis.chirp ? std::string("chirp")
                                        : FormatNumber(options.pre_emphasis.mu);
  }
  return {};
}

}  // namespace speak5220::core::encoder
