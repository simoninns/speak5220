/*
 * File:        phrase_encoder.h
 * Module:      encoder_panel
 * Purpose:     Encodes a phrase and measures the result for the GUI: the
 *              cached encoding with its PESQ and anchor, the peak code and
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
#include "core/quality/anchor_vocoder.h"
#include "core/quality/quality_metric.h"
#include "core/synth/synth.h"
#include "core/util/clock.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"

namespace speak5220::app {

// What the GUI encodes and measures with (plan §3.3, §5). The encoder and
// the quality metric (PESQ) must be thread-safe: they run on worker
// threads. `optimiser_driver` runs Optimise's search, one at a time; its
// candidates go to the window's ITaskRunner. `clock` times the debounce
// of re-encoding. Not owned; all must outlive the window.
struct EncodingTools {
  core::encoder::IEncoder* encoder = nullptr;
  core::quality::IQualityMetric* quality = nullptr;
  core::quality::IAnchorScorer* anchor = nullptr;
  core::IClock* clock = nullptr;
  core::ITaskRunner* optimiser_driver = nullptr;
};

// What is known of an encoding beyond its frames.
struct EncodingFacts {
  std::optional<double> pesq;
  std::optional<double> anchor;
  // The largest output code, from the chip's output through `synth`.
  std::optional<int> peak_code;
  // What the limiter did, known only for encodings made here.
  std::optional<core::encoder::LevelStats> levels;
};

// A new encoding with what was measured of it.
struct MeasuredEncoding {
  // PESQ and the anchor are stored in it too, when measured.
  core::project::CachedEncoding encoding;
  EncodingFacts facts;
};

// Encodes `audio` (8 kHz) with `options`, then measures the frames
// (MeasureFrames()). Fails if the encoder fails; a failed measurement
// only leaves its value unknown. On a worker thread.
core::StatusOr<MeasuredEncoding> EncodePhrase(
    const EncodingTools& tools, core::synth::ISynth& synth,
    std::span<const float> audio, const core::encoder::EncoderOptions& options,
    std::optional<double> anchor);

// Measures `frames` made from `audio`: PESQ, the anchor (unless given),
// and the peak code. Unknown values stay unset. On a worker thread.
EncodingFacts MeasureFrames(const EncodingTools& tools,
                            core::synth::ISynth& synth,
                            std::span<const float> audio,
                            const std::vector<core::lpc::Frame>& frames,
                            std::optional<double> anchor);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_PHRASE_ENCODER_H_
