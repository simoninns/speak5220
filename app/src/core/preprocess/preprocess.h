/*
 * File:        preprocess.h
 * Module:      preprocess
 * Purpose:     The preprocessing of a phrase's recording before it is
 *              encoded: the squelch, the bass cut and the treble tilt, and
 *              their presets
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
#include "core/dsp/squelch.h"

namespace speak5220::core::preprocess {

// The rate of the audio preprocessed: a project's (plan §3.1).
inline constexpr int kPreprocessSampleRate = 8000;

// How a phrase's recording is preprocessed (plan Phase 11). The encoding
// is made from the preprocessed recording and measured against it. Each
// option's range and help text are with the encoder's options
// (encoder::EncoderOptionInfos(), the preprocessing stage). All off by
// default. A plain value type.
struct PreprocessOptions {
  // If set, stretches this many dB below the recording's loudest sound,
  // such as breaths, are made silent (dsp::Squelch()).
  std::optional<double> squelch_db = std::nullopt;
  // If set, the bass is cut below this many Hz: ButterworthHighPass()
  // forwards and backwards (zero phase, so -6 dB at the cut and 24 dB per
  // octave below it).
  std::optional<double> bass_cut_hz = std::nullopt;
  // If above 0, the treble is lifted by 1 - treble_tilt z^-1.
  double treble_tilt = 0.0;

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
// spectrum of Acorn's PHROM phrases; HLD §9).
std::span<const PreprocessPreset> PreprocessPresets();

// The preset whose options are `options`, or nullptr (a custom set).
const PreprocessPreset* MatchingPreset(const PreprocessOptions& options);

// A recording preprocessed.
struct PreprocessedAudio {
  // The preprocessed recording: what the encoder aims at and an encoding
  // is measured against.
  std::vector<double> samples;
  // The recording squelched but not filtered: what the encoder finds the
  // speech's extent, pitch and voicing in, since the bass cut and treble
  // tilt would make voiced sounds look unvoiced. The same as samples when
  // neither filter is on.
  std::vector<double> voicing;
};

// Preprocesses `original` (8 kHz): squelched, then bass cut, then treble
// tilted, as `options` asks. With everything off, both are copies of
// `original`. The options must be valid (encoder::ValidateEncoderOptions()).
// Pure; O(n).
PreprocessedAudio Preprocess(std::span<const double> original,
                             const PreprocessOptions& options);

// The stretches of `original` (8 kHz) that the squelch lowers, for showing
// what it mutes; none without a squelch. Pure; O(n).
std::vector<dsp::SampleSpan> SquelchedSpans(std::span<const double> original,
                                            const PreprocessOptions& options);

// The order-2 Butterworth high-pass at `cutoff_hz` (8 kHz), by the
// bilinear transform with the corner prewarped, as scipy.signal.butter()
// designs it, with its steady-state initial conditions. Pure.
dsp::SosSection ButterworthHighPass(double cutoff_hz);

}  // namespace speak5220::core::preprocess

#endif  // SPEAK5220_CORE_PREPROCESS_PREPROCESS_H_
