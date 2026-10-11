/*
 * File:        encoding_summary.h
 * Module:      encoder_panel
 * Purpose:     The encoder panel's text: frame statistics, levels, the
 *              PESQ score against its anchor, and changed options
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

// Plan §3.3, "Presenting PESQ to the user": "PESQ 2.08 (LPC best for this
// phrase 2.31): Good", with the band's label; "PESQ 2.08" with no anchor;
// "Quality not measured" with no score. Pure.
QString QualityText(std::optional<double> pesq, std::optional<double> anchor);

// The scale's tooltip: what PESQ measures, why 4.5 is out of reach, and
// that it ranks encodings rather than predicting a listener's score.
QString QualityToolTip();

// The options that differ between `from` and `to`, by label with their
// new values ("Target RMS 22, Pre-emphasis 0.9"), or "no change". Pure.
QString ChangesText(const core::encoder::EncoderOptions& from,
                    const core::encoder::EncoderOptions& to);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_ENCODING_SUMMARY_H_
