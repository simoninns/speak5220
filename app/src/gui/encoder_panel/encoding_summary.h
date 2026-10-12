/*
 * File:        encoding_summary.h
 * Module:      encoder_panel
 * Purpose:     The encoder panel's text: frame statistics, levels, the
 *              overall quality, its figures, and changed options
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ENCODER_PANEL_ENCODING_SUMMARY_H_
#define SPEAK5220_GUI_ENCODER_PANEL_ENCODING_SUMMARY_H_

#include <QString>
#include <optional>

#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/lpc/frame_statistics.h"
#include "core/quality/quality_measures.h"

namespace speak5220::app {

// "41 frames, 1248 bits (156 B), 1217 bit/s". Pure.
QString FramesText(const core::lpc::FrameStatistics& stats);

// "voiced 48% · unvoiced 21% · repeat 19% · silence 12%", or empty with
// no frames. Pure.
QString KindsText(const core::lpc::FrameStatistics& stats);

// The output's loudest code and what the limiter did, as known:
// "peak code 112 of 127 · limiter lowered 3 frames, silenced 1", or empty
// if neither is known. Pure.
QString LevelText(std::optional<int> peak_code,
                  const std::optional<core::encoder::LevelStats>& levels);

// Plan §3.3, "Presenting quality to the user": the rating and the overall
// score, "Good · 0.72"; "Quality not measured" with no score. Pure.
QString QualityText(std::optional<double> overall);

// The figures behind the components: "envelope 3.34 dB, 28% over 4 dB ·
// pitch 5.6% gross, 60 cents RMS · voicing 9.9% · ESTOI 0.76" (ESTOI
// "too short" when it could not be measured). Pure.
QString FiguresText(const core::quality::QualityMeasurement& m);

// The quality's tooltip: what each measure compares, that only
// speech-active frames count, and how the score and rating are made.
QString QualityToolTip();

// The options that differ between `from` and `to`, by label with their
// new values ("Target RMS 22, Pre-emphasis 0.9"), or "no change". Pure.
QString ChangesText(const core::encoder::EncoderOptions& from,
                    const core::encoder::EncoderOptions& to);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_ENCODING_SUMMARY_H_
