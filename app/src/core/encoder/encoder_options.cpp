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
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <string>
#include <string_view>

#include "core/util/number_text.h"
#include "core/util/status.h"

namespace speak5220::core::encoder {
namespace {

using enum EncoderOptionType;
using enum OptionStage;

// speak5220 datasheet §12.6 and the encoder's documentation
// (docs/application/reference/encoder-options.md).
constexpr std::array<EncoderOptionInfo, 18> kInfos = {{
    {.key = "noiseGateDb",
     .field = "noise_gate_db",
     .label = "Noise gate",
     .help = "If set, sound quieter than this many dB below the recording's "
             "loudest, such as breaths and room noise, is made silent.",
     .stage = kPreprocess,
     .type = kOptionalReal,
     .minimum = 6.0,
     .maximum = 80.0,
     .default_text = "",
     .unit = "dB",
     .typical_minimum = 12.0,
     .typical_maximum = 60.0,
     .step = 1.0},
    {.key = "noiseGateMs",
     .field = "noise_gate_ms",
     .label = "Gate speed",
     .help = "How long the noise gate takes to close after a sound ends; it "
             "opens, just before a sound starts, in a quarter of this. "
             "Shorter mutes more of the gaps; longer keeps soft word "
             "endings.",
     .stage = kPreprocess,
     .type = kReal,
     .minimum = 5.0,
     .maximum = 2000.0,
     .default_text = "100",
     .unit = "ms",
     .typical_minimum = 20.0,
     .typical_maximum = 500.0,
     .step = 10.0,
     .enabled_by = "noise_gate_db"},
    {.key = "bassCutHz",
     .field = "bass_cut_hz",
     .label = "Bass cut",
     .help = "If set, the bass is cut below this frequency (24 dB per "
             "octave). Speech made brighter this way is louder and clearer "
             "on a small speaker, as Acorn's own phrases are.",
     .stage = kPreprocess,
     .type = kOptionalReal,
     .minimum = 50.0,
     .maximum = 600.0,
     .default_text = "",
     .unit = "Hz",
     .typical_minimum = 100.0,
     .typical_maximum = 400.0,
     .step = 10.0},
    {.key = "trebleTilt",
     .field = "treble_tilt",
     .label = "Treble tilt",
     .help = "Lifts the treble by 1 - mu z^-1; 0 is off.",
     .stage = kPreprocess,
     .type = kReal,
     .minimum = 0.0,
     .maximum = 0.95,
     .default_text = "0",
     .unit = "",
     .typical_minimum = 0.0,
     .typical_maximum = 0.9,
     .step = 0.05,
     .off_text = "0"},
    {.key = "deEssDb",
     .field = "de_ess_db",
     .label = "De-esser",
     .help = "If set, the hiss of S and SH sounds (the band above 2.5 kHz) "
             "is turned down by up to this many dB: by all of it where the "
             "hiss is all of the sound, and not at all where the band is "
             "this far below the whole, as in vowels.",
     .stage = kPreprocess,
     .type = kOptionalReal,
     .minimum = 1.0,
     .maximum = 30.0,
     .default_text = "",
     .unit = "dB",
     .typical_minimum = 3.0,
     .typical_maximum = 18.0,
     .step = 1.0},
    {.key = "normaliseDb",
     .field = "normalise_db",
     .label = "Normaliser",
     .help = "If set, the speech is brought to this loudness (its average "
             "power, leaving out pauses), relative to full scale, so "
             "recordings normalised to the same level sound as loud as each "
             "other; the louder stretches are compressed and the peaks "
             "limited to 1 dB below full scale.",
     .stage = kPreprocess,
     .type = kOptionalReal,
     .minimum = -40.0,
     .maximum = -6.0,
     .default_text = "",
     .unit = "dBFS",
     .typical_minimum = -30.0,
     .typical_maximum = -12.0,
     .step = 1.0},
    {.key = "normaliseRatio",
     .field = "normalise_ratio",
     .label = "Compression",
     .help = "How much the normaliser evens out the speech: over nearly all "
             "of it (from 10 dB under its level), a sound this many dB "
             "louder comes out only 1 dB louder, with a soft knee. 1 only "
             "sets the level and limits the peaks.",
     .stage = kPreprocess,
     .type = kReal,
     .minimum = 1.0,
     .maximum = 20.0,
     .default_text = "3",
     .unit = ":1",
     .typical_minimum = 1.0,
     .typical_maximum = 8.0,
     .step = 0.5,
     .enabled_by = "normalise_db"},
    {.key = "targetRms",
     .field = "target_rms",
     .label = "Target RMS",
     .help = "RMS of the output over sounding frames, in DAC codes, "
             "measured after a 279 Hz high-pass (the BBC Micro's), so "
             "bass does not count.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 100.0,
     .default_text = "20",
     .unit = "codes",
     .typical_minimum = 5.0,
     .typical_maximum = 50.0,
     .step = 0.5},
    {.key = "repeatLambda",
     .field = "repeat_lambda",
     .label = "Repeat trade-off",
     .help = "Bits a run of repeats may save per dB of extra prediction "
             "error. Lower gives fewer bits.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 1000.0,
     .default_text = "300",
     .unit = "bits/dB",
     .typical_minimum = 0.0,
     .typical_maximum = 600.0,
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
     .step = 0.5,
     .advanced = true},
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
     .default_text = "256",
     .unit = "samples",
     .typical_minimum = 120.0,
     .typical_maximum = 400.0,
     .step = 8.0,
     .advanced = true},
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
     .step = 5.0,
     .advanced = true},
    {.key = "resonanceHoldDb",
     .field = "resonance_hold_db",
     .label = "Resonance hold",
     .help = "If set, where a fading voiced sound moves from a strongly "
             "resonant filter to one with this many dB less gain, the "
             "resonant filter is kept for one more frame. Without it the "
             "chip's interpolation drops the level suddenly, heard as a "
             "pop.",
     .type = kOptionalReal,
     .minimum = 3.0,
     .maximum = 40.0,
     .default_text = "14",
     .unit = "dB",
     .typical_minimum = 6.0,
     .typical_maximum = 30.0,
     .step = 0.5,
     .advanced = true},
    {.key = "voicingThreshold",
     .field = "voicing_threshold",
     .label = "Voicing threshold",
     .help = "Periodicity (CMNDF) level at which voiced and unvoiced cost "
             "the same. Higher gives more voiced frames.",
     .type = kReal,
     .minimum = 0.0,
     .maximum = 1.0,
     .default_text = "0.6",
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
     .default_text = "5",
     .unit = "",
     .typical_minimum = 0.0,
     .typical_maximum = 12.0,
     .step = 0.25,
     .advanced = true},
    {.key = "voicingMinRun",
     .field = "voicing_min_run",
     .label = "Shortest voicing run",
     .help = "A run of voiced or unvoiced frames shorter than this, between "
             "runs of the other kind, takes its neighbours' voicing, so the "
             "sound does not flick between buzz and hiss. 1 is off.",
     .type = kInteger,
     .minimum = 1.0,
     .maximum = 8.0,
     .default_text = "2",
     .unit = "frames",
     .typical_minimum = 1.0,
     .typical_maximum = 6.0,
     .step = 1.0,
     .advanced = true,
     .off_text = "1"},
    {.key = "levelAgc",
     .field = "level_agc",
     .label = "Slow gain control",
     .help = "Compression ratio of a slow (0.75 s) loudness envelope; "
             "brings quiet words up. 1 is off.",
     .type = kReal,
     .minimum = 1.0,
     .maximum = 10.0,
     .default_text = "1.25",
     .unit = ":1",
     .typical_minimum = 1.0,
     .typical_maximum = 6.0,
     .step = 0.05,
     .advanced = true},
}};

// The fields of each type, by name, for SetEncoderOption() and
// EncoderOptionText(), with where each is kept. pre_emphasis has a type of
// its own.
template <typename T>
struct Field {
  std::string_view field;
  T& (*member)(EncoderOptions&);
};
using RealField = Field<double>;
using IntegerField = Field<int>;
using OptionalRealField = Field<std::optional<double>>;

constexpr std::array<RealField, 10> kRealFields = {{
    {.field = "noise_gate_ms",
     .member = [](EncoderOptions& o) -> double& {
       return o.preprocess.noise_gate_ms;
     }},
    {.field = "treble_tilt",
     .member = [](EncoderOptions& o) -> double& {
       return o.preprocess.treble_tilt;
     }},
    {.field = "normalise_ratio",
     .member = [](EncoderOptions& o) -> double& {
       return o.preprocess.normalise_ratio;
     }},
    {.field = "target_rms",
     .member = [](EncoderOptions& o) -> double& { return o.target_rms; }},
    {.field = "repeat_lambda",
     .member = [](EncoderOptions& o) -> double& { return o.repeat_lambda; }},
    {.field = "gate_db",
     .member = [](EncoderOptions& o) -> double& { return o.gate_db; }},
    {.field = "lag_window_hz",
     .member = [](EncoderOptions& o) -> double& { return o.lag_window_hz; }},
    {.field = "voicing_threshold",
     .member = [](EncoderOptions& o) -> double& {
       return o.voicing_threshold;
     }},
    {.field = "voicing_evidence",
     .member = [](EncoderOptions& o) -> double& { return o.voicing_evidence; }},
    {.field = "level_agc",
     .member = [](EncoderOptions& o) -> double& { return o.level_agc; }},
}};
constexpr std::array<IntegerField, 2> kIntegerFields = {{
    {.field = "window",
     .member = [](EncoderOptions& o) -> int& { return o.window; }},
    {.field = "voicing_min_run",
     .member = [](EncoderOptions& o) -> int& { return o.voicing_min_run; }},
}};
constexpr std::array<OptionalRealField, 5> kOptionalRealFields = {{
    {.field = "noise_gate_db",
     .member = [](EncoderOptions& o) -> std::optional<double>& {
       return o.preprocess.noise_gate_db;
     }},
    {.field = "bass_cut_hz",
     .member = [](EncoderOptions& o) -> std::optional<double>& {
       return o.preprocess.bass_cut_hz;
     }},
    {.field = "de_ess_db",
     .member = [](EncoderOptions& o) -> std::optional<double>& {
       return o.preprocess.de_ess_db;
     }},
    {.field = "normalise_db",
     .member = [](EncoderOptions& o) -> std::optional<double>& {
       return o.preprocess.normalise_db;
     }},
    {.field = "resonance_hold_db",
     .member = [](EncoderOptions& o) -> std::optional<double>& {
       return o.resonance_hold_db;
     }},
}};

// The field `field` of `options`.
template <typename T, std::size_t N>
T& FieldOf(const std::array<Field<T>, N>& table, EncoderOptions& options,
           std::string_view field) {
  for (const Field<T>& entry : table) {
    if (entry.field == field) {
      return entry.member(options);
    }
  }
  // Only called for a field of the table's type (FindEncoderOption()).
  std::abort();
}
// The value of the field `field` of `options`.
template <typename T, std::size_t N>
T ValueOf(const std::array<Field<T>, N>& table, const EncoderOptions& options,
          std::string_view field) {
  EncoderOptions copy = options;
  return FieldOf(table, copy, field);
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
    if (Status status =
            CheckRange(entry.field, ValueOf(kRealFields, options, entry.field));
        !status.IsOk()) {
      return status;
    }
  }
  for (const IntegerField& entry : kIntegerFields) {
    if (Status status = CheckRange(
            entry.field,
            static_cast<double>(ValueOf(kIntegerFields, options, entry.field)));
        !status.IsOk()) {
      return status;
    }
  }
  for (const OptionalRealField& entry : kOptionalRealFields) {
    if (const std::optional<double> value =
            ValueOf(kOptionalRealFields, options, entry.field);
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
      FieldOf(kRealFields, updated, field) = *value;
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
      FieldOf(kIntegerFields, updated, field) = static_cast<int>(*value);
      break;
    }
    case kOptionalReal:
      if (text.empty() || text == kUnsetOptionText) {
        FieldOf(kOptionalRealFields, updated, field).reset();
      } else if (const std::optional<double> value = ParseNumber(text)) {
        FieldOf(kOptionalRealFields, updated, field) = value;
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
      return FormatNumber(ValueOf(kRealFields, options, field));
    case kInteger:
      return std::to_string(ValueOf(kIntegerFields, options, field));
    case kOptionalReal: {
      const std::optional<double> value =
          ValueOf(kOptionalRealFields, options, field);
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
