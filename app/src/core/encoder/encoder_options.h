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

#include "core/preprocess/preprocess.h"
#include "core/util/status.h"

namespace speak5220::core::encoder {

// Everything that controls a phrase's encoding: how its recording is
// preprocessed (preprocess, applied by preprocess::Preprocess() before the
// encoder's stages) and the encoder's own options. The defaults are those of
// tms5220enc.py DEFAULTS but for window, voicing_evidence and level_agc,
// chosen for the best median quality score on the corpus, and
// voicing_threshold and repeat_lambda, chosen against Acorn's PHROM;
// target_rms keeps the reference's 20, of loudness (the level after
// LoudnessHighPass()) rather than level; and the options the reference
// does not have: resonance_hold_db, 14, and voicing_min_run, 2, with no
// preprocessing. The reference's other options are fixed at its defaults.
// Projects list every option, so changing a default never
// changes a project's encodings. Each field's range and help text are in
// EncoderOptionInfos(). A plain value type.
struct EncoderOptions {
  // The preprocessing: the noise gate, the bass cut, the treble tilt and
  // the de-esser. Not in the reference encoder.
  preprocess::PreprocessOptions preprocess;
  // RMS of the output over sounding frames, in codes, after the loudness
  // high-pass (LoudnessHighPass()), so bass does not count. The
  // reference's is 20 too, but of the plain output; 20 gives Acorn's
  // PHROM's own words its loudness.
  double target_rms = 20.0;
  // Bits a repeat run may trade per dB of prediction error. The
  // reference's is 70; 300 comes near Acorn's PHROM's repeats.
  double repeat_lambda = 300.0;
  // Frames this many dB below the loudest are silence.
  double gate_db = 45.0;
  // Analysis window, in samples (32 ms).
  int window = 256;
  // Bandwidth of the Gaussian lag window, in Hz.
  double lag_window_hz = 60.0;
  // If set, a voiced frame whose filter gain is more than this many dB
  // below the voiced frame before it, as the sound fades, repeats that
  // frame's filter (HoldResonantFilters()), so the level does not fall
  // suddenly (a pop). Not in the reference encoder; 14 catches the pops
  // heard on the corpus at little cost to its score.
  std::optional<double> resonance_hold_db = 14.0;
  // CMNDF level at which voiced and unvoiced cost the same. The
  // reference's is 0.3; 0.6 voices Acorn's PHROM most like its own frames
  // without voicing its hiss.
  double voicing_threshold = 0.6;
  // Weight of spectral tilt and zero crossings in the voicing threshold.
  double voicing_evidence = 5.0;
  // Runs of voiced or unvoiced frames shorter than this, between runs of
  // the other kind, take their neighbours' voicing (SmoothVoicingRuns());
  // 1 is off. Not in the reference encoder; 2, which removes one-frame
  // islands, brings the voicing nearer Acorn's PHROM's at no cost on the
  // corpus.
  int voicing_min_run = 2;
  // Slow gain control ratio over about 0.75 s (1: off).
  double level_agc = 1.25;

  friend bool operator==(const EncoderOptions& a,
                         const EncoderOptions& b) = default;
};

// The stage an option belongs to: the preprocessing of the
// recording, whose result an encoding is measured against, or the
// encoder.
enum class OptionStage {
  kPreprocess,
  kEncoder,
};

// How an option is edited.
enum class EncoderOptionType {
  kReal,
  kInteger,
  // A real value that may also be unset (noise_gate_db, bass_cut_hz,
  // de_ess_db, resonance_hold_db).
  kOptionalReal,
};

// What the user interface needs to show and check one option.
struct EncoderOptionInfo {
  // The option's name in the reference encoder, tms5220enc.py (targetRms,
  // ...), as its documentation and the reference data use it; for an
  // option the reference does not have (noise_gate_db), a name in its
  // style.
  std::string_view key;
  // The name in project files and on the command line: the EncoderOptions
  // field (target_rms, ...), as project files name YAML keys.
  std::string_view field;
  std::string_view label;
  std::string_view help;
  OptionStage stage = OptionStage::kEncoder;
  EncoderOptionType type = EncoderOptionType::kReal;
  // The valid range, inclusive.
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
  // project file or Optimise, widens the slider).
  double typical_minimum = 0.0;
  double typical_maximum = 0.0;
  double step = 0.0;
  // A fine adjustment whose default is nearly always right: the GUI shows
  // it only with its advanced settings. The others have the
  // clearest effect on how a phrase sounds or how big it is.
  bool advanced = false;
  // The value at which the option has no effect, as EncoderOptionText()
  // gives it, which the GUI shows as "off"; empty for an option that has
  // none, or one that can be unset (kOptionalReal) instead.
  std::string_view off_text = {};
  // The optional option (kOptionalReal) without which this one has no
  // effect, which the GUI disables while that one is unset
  // (noise_gate_ms needs noise_gate_db); empty for none.
  std::string_view enabled_by = {};
};

// One entry per option, in the order of the struct: the preprocessing
// options first.
std::span<const EncoderOptionInfo> EncoderOptionInfos();

// Checks every option against its range. The encoder refuses options that
// fail this.
Status ValidateEncoderOptions(const EncoderOptions& options);

// The option whose field name (EncoderOptionInfo::field) is `field`, or
// nullptr.
const EncoderOptionInfo* FindEncoderOption(std::string_view field);

// The text for an unset optional option (noise_gate_db, bass_cut_hz,
// de_ess_db, resonance_hold_db).
inline constexpr std::string_view kUnsetOptionText = "none";

// Sets one option from text, as a user types it or a project file holds
// it: a number ("18", "0.35"), and a number or "none" for noise_gate_db,
// bass_cut_hz, de_ess_db and resonance_hold_db.
// Numbers use '.' as the decimal point, whatever the locale. Fails,
// leaving `options` unchanged, for an unknown field, text of the wrong
// type, a fractional value for a whole-number option, or a value outside
// the option's range.
Status SetEncoderOption(EncoderOptions& options, std::string_view field,
                        std::string_view text);

// The text of one option, which SetEncoderOption() reads back to the same
// value: the shortest exact number, or "none".
// Returns an empty string for an unknown field.
std::string EncoderOptionText(const EncoderOptions& options,
                              std::string_view field);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_ENCODER_OPTIONS_H_
