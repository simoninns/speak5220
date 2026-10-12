/*
 * File:        phrase_encoder.h
 * Module:      encoder_panel
 * Purpose:     Encodes a phrase and measures the result for the GUI: the
 *              cached encoding with its quality, the peak code and
 *              the limiter's activity
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ENCODER_PANEL_PHRASE_ENCODER_H_
#define SPEAK5220_GUI_ENCODER_PANEL_PHRASE_ENCODER_H_

#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/synth.h"
#include "core/util/clock.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"

namespace speak5220::app {

// What the GUI encodes and measures with (plan §3.3, §5). The encoder and
// the quality assessor must be thread-safe: they run on worker threads;
// Optimise maximises the assessor's overall score. `optimiser_driver` runs
// Optimise's search, one at a time; its candidates go to the window's
// ITaskRunner. `clock` times the debounce of re-encoding. Not owned; all must
// outlive the window.
struct EncodingTools {
  core::encoder::IEncoder* encoder = nullptr;
  core::quality::IQualityAssessor* quality = nullptr;
  core::IClock* clock = nullptr;
  core::ITaskRunner* optimiser_driver = nullptr;
};

// What is known of an encoding beyond its frames.
struct EncodingFacts {
  // The quality measures and the overall score.
  std::optional<core::quality::QualityAssessment> quality;
  // The largest output code, from the chip's output through `synth`.
  std::optional<int> peak_code;
  // What the limiter did, known only for encodings made here.
  std::optional<core::encoder::LevelStats> levels;
};

// A new encoding with what was measured of it.
struct MeasuredEncoding {
  // The quality is stored in it too, when measured.
  core::project::CachedEncoding encoding;
  EncodingFacts facts;
};

// Encodes `audio` (8 kHz) with `options`, then measures the frames
// (MeasureFrames()). Fails if the encoder fails; a failed measurement only
// leaves its value unknown. On a worker thread.
core::StatusOr<MeasuredEncoding> EncodePhrase(
    const EncodingTools& tools, core::synth::ISynth& synth,
    std::span<const float> audio, const core::encoder::EncoderOptions& options);

// Measures `frames` made from `audio`: the quality, against `audio`
// preprocessed by `options` (preprocess::Preprocess()) as the encoder
// aimed at it, and the peak code. Unknown values stay unset. On a
// worker thread.
EncodingFacts MeasureFrames(const EncodingTools& tools,
                            core::synth::ISynth& synth,
                            std::span<const float> audio,
                            const core::encoder::EncoderOptions& options,
                            const std::vector<core::lpc::Frame>& frames);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_PHRASE_ENCODER_H_
