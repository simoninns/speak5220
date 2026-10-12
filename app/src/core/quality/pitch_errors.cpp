/*
 * File:        pitch_errors.cpp
 * Module:      quality
 * Purpose:     The pitch and voicing measures: gross pitch error, F0 RMSE
 *              in cents and voicing decision error between an original
 *              and the chip's output
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pitch_errors.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/util/logging.h"

namespace speak5220::core::quality {
namespace {

constexpr double kCentsPerOctave = 1200.0;

}  // namespace

PitchErrors ComparePitch(std::span<const double> original_f0,
                         std::span<const double> output_f0,
                         const std::vector<bool>& speech) {
  const std::size_t n =
      std::min({original_f0.size(), output_f0.size(), speech.size()});
  PitchErrors errors;
  int voicing_differs = 0;
  int gross = 0;
  int fine = 0;
  double cents_squared = 0.0;
  for (std::size_t i = 0; i < n; ++i) {
    if (!speech[i]) {
      continue;
    }
    ++errors.frames;
    const bool original_voiced = original_f0[i] > 0.0;
    const bool output_voiced = output_f0[i] > 0.0;
    if (original_voiced != output_voiced) {
      ++voicing_differs;
      continue;
    }
    if (!original_voiced) {
      continue;
    }
    ++errors.voiced_frames;
    const double ratio = output_f0[i] / original_f0[i];
    if (std::fabs(ratio - 1.0) > kGrossPitchRatio) {
      ++gross;
      continue;
    }
    const double cents = kCentsPerOctave * std::log2(ratio);
    cents_squared += cents * cents;
    ++fine;
  }
  if (errors.frames > 0) {
    errors.voicing_error = 100.0 * voicing_differs / errors.frames;
  }
  if (errors.voiced_frames > 0) {
    errors.gross_error = 100.0 * gross / errors.voiced_frames;
  }
  if (fine > 0) {
    errors.f0_rmse_cents = std::sqrt(cents_squared / fine);
  }
  return errors;
}

StatusOr<PitchErrors> MeasurePitchErrors(const PreparedSignals& prepared,
                                         IPitchEstimator& estimator) {
  StatusOr<std::vector<double>> original = estimator.Track(prepared.original);
  if (!original.IsOk()) {
    return original.GetStatus();
  }
  StatusOr<std::vector<double>> output = estimator.Track(prepared.output);
  if (!output.IsOk()) {
    return output.GetStatus();
  }
  const PitchErrors errors = ComparePitch(*original, *output, prepared.speech);
  Logger(kLogModuleQuality)
      ->debug(
          "pitch: gross {:.1f}%, RMSE {:.1f} cents, voicing {:.1f}% "
          "over {} frames ({} voiced in both)",
          errors.gross_error, errors.f0_rmse_cents, errors.voicing_error,
          errors.frames, errors.voiced_frames);
  return errors;
}

}  // namespace speak5220::core::quality
