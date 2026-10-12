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
#include <string>
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
// tms5220enc.py DEFAULTS but for target_rms, window, voicing_evidence and
// level_agc, chosen for the best median quality score on the corpus (HLD
// §9), and resonance_hold_db, 14, which the reference does not have. The
// reference's other options are fixed at its defaults (HLD §8, "Options").
// Projects list every option (plan §4), so changing a default never
// changes a project's encodings. Each field's range and help text are in
// EncoderOptionInfos(). A plain value type.
struct EncoderOptions {
  // RMS of the output over sounding frames, in codes.
  double target_rms = 24.0;
  // Bits a repeat run may trade per dB of prediction error.
  double repeat_lambda = 70.0;
  // Frames this many dB below the loudest are silence.
  double gate_db = 45.0;
  // If set, the source is squelched first (dsp::Squelch()): stretches this
  // many dB below its loudest sound, such as breaths, are made silent
  // before they are encoded or scored against (SquelchedSource()).
  std::optional<double> squelch_db;
  // Voiced analysis pre-emphasis.
  PreEmphasis pre_emphasis;
  // Analysis window, in samples (32 ms).
  int window = 256;
  // Bandwidth of the Gaussian lag window, in Hz.
  double lag_window_hz = 60.0;
  // If set, a voiced frame whose filter gain is more than this many dB
  // below the voiced frame before it, as the sound fades, repeats that
  // frame's filter (HoldResonantFilters()), so the level does not fall
  // suddenly (a pop). Not in the reference encoder; 14 catches the pops
  // heard on the corpus at little cost to its score (HLD §9).
  std::optional<double> resonance_hold_db = 14.0;
  // CMNDF level at which voiced and unvoiced cost the same.
  double voicing_threshold = 0.3;
  // Weight of spectral tilt and zero crossings in the voicing threshold.
  double voicing_evidence = 5.0;
  // Slow gain control ratio over about 0.75 s (1: off).
  double level_agc = 1.25;

  friend bool operator==(const EncoderOptions& a,
                         const EncoderOptions& b) = default;
};

// How an option is edited.
enum class EncoderOptionType {
  kReal,
  kInteger,
  // A real value that may also be unset (squelch_db, resonance_hold_db).
  kOptionalReal,
  // "chirp" or a real coefficient (pre_emphasis).
  kPreEmphasis,
};

// What the user interface needs to show and check one option.
struct EncoderOptionInfo {
  // The option's name in the reference encoder, tms5220enc.py (targetRms,
  // ...), as its documentation and the reference data use it; for an
  // option the reference does not have (squelch_db), a name in its style.
  std::string_view key;
  // The name in project files and on the command line: the EncoderOptions
  // field (target_rms, ...), as plan §4 names YAML keys.
  std::string_view field;
  std::string_view label;
  std::string_view help;
  EncoderOptionType type = EncoderOptionType::kReal;
  // The valid range, inclusive; for kPreEmphasis, the range of mu.
  double minimum = 0.0;
  double maximum = 0.0;
  // The default, as text a user would type.
  std::string_view default_text;
  // The unit of the option's values, as the GUI shows it after a value
  // ("codes", "dB"); empty for none. ":1" marks a ratio, whose minimum (1)
  // turns it off.
  std::string_view unit;
  // Where useful values lie, within minimum..maximum, and the step
  // between them: what the GUI's sliders offer (a value outside, from a
  // project file or Optimise, widens the slider). For kPreEmphasis, of
  // mu.
  double typical_minimum = 0.0;
  double typical_maximum = 0.0;
  double step = 0.0;
  // A fine adjustment whose default is nearly always right: the GUI shows
  // it only with its advanced settings. The others have the
  // clearest effect on how a phrase sounds or how big it is.
  bool advanced = false;
};

// One entry per EncoderOptions field, in the order of the struct.
std::span<const EncoderOptionInfo> EncoderOptionInfos();

// Checks every option against its range. The encoder refuses options that
// fail this.
Status ValidateEncoderOptions(const EncoderOptions& options);

// The option whose field name (EncoderOptionInfo::field) is `field`, or
// nullptr.
const EncoderOptionInfo* FindEncoderOption(std::string_view field);

// The text for an unset optional option (squelch_db, resonance_hold_db).
inline constexpr std::string_view kUnsetOptionText = "none";

// Sets one option from text, as a user types it or a project file holds
// it: a number ("18", "0.35"), "chirp" or a number for pre_emphasis, and
// a number or "none" for squelch_db and resonance_hold_db.
// Numbers use '.' as the decimal point, whatever the locale. Fails,
// leaving `options` unchanged, for an unknown field, text of the wrong
// type, a fractional value for a whole-number option, or a value outside
// the option's range.
Status SetEncoderOption(EncoderOptions& options, std::string_view field,
                        std::string_view text);

// The text of one option, which SetEncoderOption() reads back to the same
// value: the shortest exact number, "chirp", or "none".
// Returns an empty string for an unknown field.
std::string EncoderOptionText(const EncoderOptions& options,
                              std::string_view field);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_ENCODER_OPTIONS_H_
