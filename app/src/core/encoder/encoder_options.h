/*
 * File:        encoder_options.h
 * Module:      encoder
 * Purpose:     The encoder's options, with the range, default and help text
 *              the user interface shows for each
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_ENCODER_OPTIONS_H_
#define SPEAK5220_CORE_ENCODER_ENCODER_OPTIONS_H_

#include <optional>
#include <span>
#include <string_view>

#include "core/util/status.h"

namespace speak5220::core::encoder {

// The voiced analysis pre-emphasis (speak5220 datasheet §12.3): the FIR
// that whitens the chirp, or 1 - mu z^-1.
struct PreEmphasis {
  // true: the order-4 LPC inverse of the chirp; false: 1 - mu z^-1.
  bool chirp = true;
  double mu = 0.0;

  static PreEmphasis Chirp() { return {}; }
  static PreEmphasis FirstOrder(double mu) {
    return {.chirp = false, .mu = mu};
  }

  friend bool operator==(const PreEmphasis& a, const PreEmphasis& b) = default;
};

// Everything that controls an encoding. The defaults are those of
// tms5220enc.py DEFAULTS; each field's range and help text are in
// EncoderOptionInfos(). A plain value type.
struct EncoderOptions {
  // RMS of the output over sounding frames, in codes.
  double target_rms = 20.0;
  // If set, scale so the loudest output peak is this instead of target_rms.
  std::optional<double> target_peak;
  // No output code beyond this (the DAC's range is +-127).
  double peak_limit = 120.0;
  // Use repeat frames.
  bool repeats = true;
  // Bits a repeat run may trade per dB of prediction error.
  double repeat_lambda = 70.0;
  // Frames this many dB below the loudest are silence.
  double gate_db = 45.0;
  // Voiced analysis pre-emphasis.
  PreEmphasis pre_emphasis;
  // Analysis window, in samples (30 ms).
  int window = 240;
  // Bandwidth of the Gaussian lag window, in Hz.
  double lag_window_hz = 60.0;
  // CMNDF level at which voiced and unvoiced cost the same.
  double voicing_threshold = 0.3;
  // Weight of spectral tilt and zero crossings in the voicing threshold.
  double voicing_evidence = 4.0;
  // Closed-loop energy passes.
  int iterations = 5;
  // Fade unvoiced sound before a stop or silence (§12.2).
  bool fade_unvoiced = true;
  // Slow gain control ratio over about 0.75 s (1: off).
  double level_agc = 1.0;
  // Per-frame level compression ratio (1: off).
  double compression = 1.0;

  friend bool operator==(const EncoderOptions& a,
                         const EncoderOptions& b) = default;
};

// How an option is edited.
enum class EncoderOptionType {
  kReal,
  kInteger,
  kBoolean,
  // A real value that may also be unset (target_peak).
  kOptionalReal,
  // "chirp" or a real coefficient (pre_emphasis).
  kPreEmphasis,
};

// What the user interface needs to show and check one option.
struct EncoderOptionInfo {
  // The key in project files and on the command line: the Python option
  // name (targetRms, ...).
  std::string_view key;
  std::string_view label;
  std::string_view help;
  EncoderOptionType type = EncoderOptionType::kReal;
  // The valid range, inclusive; for kPreEmphasis, the range of mu.
  double minimum = 0.0;
  double maximum = 0.0;
  // The default, as text a user would type.
  std::string_view default_text;
};

// One entry per EncoderOptions field, in the order of the struct.
std::span<const EncoderOptionInfo> EncoderOptionInfos();

// Checks every option against its range. The encoder refuses options that
// fail this.
Status ValidateEncoderOptions(const EncoderOptions& options);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_ENCODER_OPTIONS_H_
