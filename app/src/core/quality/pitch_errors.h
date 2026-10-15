/*
 * File:        pitch_errors.h
 * Module:      quality
 * Purpose:     The pitch and voicing measures: gross pitch error, F0 RMSE
 *              in cents and voicing decision error between an original
 *              and the chip's output
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_PITCH_ERRORS_H_
#define SPEAK5220_CORE_QUALITY_PITCH_ERRORS_H_

#include <span>
#include <vector>

#include "core/quality/pitch_estimator.h"
#include "core/quality/signal_preparation.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// An output F0 more than this fraction away from the original's is a gross
// error.
inline constexpr double kGrossPitchRatio = 0.2;

// The pitch and voicing measures of an encoding.
struct PitchErrors {
  // The percentage of the frames voiced in both whose F0 differs by more
  // than kGrossPitchRatio.
  double gross_error = 0.0;
  // The RMS difference in cents over the other frames voiced in both.
  double f0_rmse_cents = 0.0;
  // The percentage of the frames counted whose voicing differs.
  double voicing_error = 0.0;
  // The frames counted, and of those, the frames voiced in both.
  int frames = 0;
  int voiced_frames = 0;
};

// The measures between two F0 tracks (Hz, 0 where unvoiced, every
// kPitchHop samples) over the frames `speech` counts: track frame i is
// counted if mask frame i is speech. With no frame voiced in both, the
// gross error and RMSE are 0. Pure; thread-safe.
PitchErrors ComparePitch(std::span<const double> original_f0,
                         std::span<const double> output_f0,
                         const std::vector<bool>& speech);

// Tracks both of `prepared`'s signals with `estimator` and compares them.
// Fails if the estimator does. Thread-safe if the estimator is.
StatusOr<PitchErrors> MeasurePitchErrors(const PreparedSignals& prepared,
                                         IPitchEstimator& estimator);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_PITCH_ERRORS_H_
