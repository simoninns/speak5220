/*
 * File:        preprocess.h
 * Module:      preprocess
 * Purpose:     The preprocessing of a phrase's recording before it is
 *              encoded: the noise gate, the bass cut, the treble tilt and
 *              the de-esser, and their presets
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PREPROCESS_PREPROCESS_H_
#define SPEAK5220_CORE_PREPROCESS_PREPROCESS_H_

#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/dsp/iir_filter.h"
#include "core/dsp/noise_gate.h"

namespace speak5220::core::preprocess {

// The rate of the audio preprocessed: a project's.
inline constexpr int kPreprocessSampleRate = 8000;

// The noise gate's default speed: how long it takes to close, in ms.
inline constexpr double kDefaultNoiseGateMs = 100.0;

// The lower edge of the band the de-esser turns down, in Hz: where an S's
// hiss is, of what an 8 kHz recording keeps of it.
inline constexpr double kDeEssBandHz = 2500.0;

// How a phrase's recording is preprocessed. The encoding
// is made from the preprocessed recording and measured against it. Each
// option's range and help text are with the encoder's options
// (encoder::EncoderOptionInfos(), the preprocessing stage). All off by
// default. A plain value type.
struct PreprocessOptions {
  // If set, stretches quieter than this many dB below the recording's
  // loudest sound, such as breaths, are turned down, those well below it
  // to silence (dsp::NoiseGate()).
  std::optional<double> noise_gate_db = std::nullopt;
  // How long the noise gate takes to close, in ms, after the sound falls;
  // it opens in a quarter of this. No effect without noise_gate_db.
  double noise_gate_ms = kDefaultNoiseGateMs;
  // If set, the bass is cut below this many Hz: ButterworthHighPass()
  // forwards and backwards (zero phase, so -6 dB at the cut and 24 dB per
  // octave below it).
  std::optional<double> bass_cut_hz = std::nullopt;
  // If above 0, the treble is lifted by 1 - treble_tilt z^-1.
  double treble_tilt = 0.0;
  // If set, the band above kDeEssBandHz is turned down by up to this many
  // dB where it is most of the sound, as in an S (dsp::DeEss()).
  std::optional<double> de_ess_db = std::nullopt;

  friend bool operator==(const PreprocessOptions& a,
                         const PreprocessOptions& b) = default;
};

// A named set of preprocessing options the user can choose.
struct PreprocessPreset {
  std::string_view name;
  PreprocessOptions options;
};

// The presets, in the order offered: "None" (everything off) and "BBC
// Micro" (a bass cut at 200 Hz and a treble tilt of 0.4, like the
// spectrum of Acorn's PHROM phrases).
std::span<const PreprocessPreset> PreprocessPresets();

// The preset whose options have the same effect as `options`, or nullptr
// (a custom set). The noise gate's speed is ignored while the gate is off.
const PreprocessPreset* MatchingPreset(const PreprocessOptions& options);

// A recording preprocessed.
struct PreprocessedAudio {
  // The preprocessed recording: what the encoder aims at and an encoding
  // is measured against.
  std::vector<double> samples;
  // The recording gated but not filtered or de-essed: what the encoder
  // finds the speech's extent, pitch and voicing in, since the bass cut,
  // the treble tilt and the de-esser would change how voiced a sound
  // looks. The same as samples when none of them is on.
  std::vector<double> voicing;
};

// Preprocesses `original` (8 kHz) as `options` asks: gated, then bass cut,
// treble tilted and de-essed. With everything off, both are copies of
// `original`. The options must be valid (encoder::ValidateEncoderOptions()).
// Pure; O(n).
PreprocessedAudio Preprocess(std::span<const double> original,
                             const PreprocessOptions& options);

// The stretches of `original` (8 kHz) that the noise gate turns down by
// more than 6 dB (dsp::GatedSpans()), for showing what it mutes; none
// without a noise gate. Pure; O(n).
std::vector<dsp::SampleSpan> GatedSpans(std::span<const double> original,
                                        const PreprocessOptions& options);

// The order-2 Butterworth high-pass at `cutoff_hz` (8 kHz), by the
// bilinear transform with the corner prewarped, as scipy.signal.butter()
// designs it, with its steady-state initial conditions. Pure.
dsp::SosSection ButterworthHighPass(double cutoff_hz);

}  // namespace speak5220::core::preprocess

#endif  // SPEAK5220_CORE_PREPROCESS_PREPROCESS_H_
