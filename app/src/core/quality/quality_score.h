/*
 * File:        quality_score.h
 * Module:      quality
 * Purpose:     The overall quality score and rating: the measures mapped to
 *              component scores and combined by a weighted geometric mean
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_QUALITY_SCORE_H_
#define SPEAK5220_CORE_QUALITY_QUALITY_SCORE_H_

#include <optional>
#include <string_view>

#include "core/quality/quality_measures.h"

namespace speak5220::core::quality {

// Where a figure maps to a score: 0 at or beyond `poor`, 1 at or beyond
// `good`, linear between. Either way round: for a figure where less is
// better, `good` is the smaller.
struct ScoreRange {
  double poor = 0.0;
  double good = 1.0;
};

// The calibration of the overall score, on the 243
// corpus files: each figure's good end is what the anchor vocoder
// (unquantised LPC) reaches at its median, for the envelope and ESTOI, or
// the best tenth of the default encodings, for the pitch figures (the
// anchor's own pitch tracker is cruder than the encoder's); its poor end a
// round value just beyond the worst 1 % of the encodings (the reference
// encodings of every option set, the default and the optimised), but for
// the gross pitch error, capped at half the voiced frames. The components
// weigh the same. The bands rate the anchor Good on 96 % of the files.
struct ScoreCalibration {
  // The envelope component: the mean distortion (dB) and the percentage of
  // frames over 4 dB.
  static constexpr ScoreRange kLsd = {.poor = 5.0, .good = 1.8};
  static constexpr ScoreRange kLargeOutliers = {.poor = 60.0, .good = 2.0};
  // The pitch component: gross pitch error (%), F0 RMSE (cents) and
  // voicing decision error (%).
  static constexpr ScoreRange kGrossPitch = {.poor = 50.0, .good = 0.0};
  static constexpr ScoreRange kF0Rmse = {.poor = 120.0, .good = 40.0};
  static constexpr ScoreRange kVoicing = {.poor = 30.0, .good = 3.5};
  // The intelligibility component: ESTOI.
  static constexpr ScoreRange kEstoi = {.poor = 0.55, .good = 0.82};
  // The components' weights in the geometric mean; they sum to 1.
  static constexpr double kEnvelopeWeight = 1.0 / 3.0;
  static constexpr double kPitchWeight = 1.0 / 3.0;
  static constexpr double kIntelligibilityWeight = 1.0 / 3.0;
  // The lowest overall score rated Fair, and Good.
  static constexpr double kFairFrom = 0.5;
  static constexpr double kGoodFrom = 0.7;
};

// The three component scores, each 0 to 1.
struct QualityComponents {
  double envelope = 0.0;
  double pitch = 0.0;
  // Absent when ESTOI could not be measured (too little sound).
  std::optional<double> intelligibility;
};

// The overall rating, worst first.
enum class QualityRating { kPoor, kFair, kGood };

// An encoding's score: its components, the overall score (0 to 1) and its
// rating.
struct QualityScore {
  QualityComponents components;
  double overall = 0.0;
  QualityRating rating = QualityRating::kPoor;
};

// `value` on `range`, clamped to 0..1.
double MapToScore(double value, ScoreRange range);

// The component scores of `m`: the envelope's the mean of its figures'
// scores, and the pitch's likewise.
QualityComponents ScoreComponents(const QualityMeasurement& m);

// The weighted geometric mean of the components, so a poor one pulls the
// score down and good ones cannot hide it. Without intelligibility, the
// other two share its weight in proportion.
double CombineComponents(const QualityComponents& components);

// The rating of an overall score.
QualityRating RatingOf(double overall);

// The rating's label, as the GUI and the CLI show it.
std::string_view RatingLabel(QualityRating rating);

// The score of `m`. Pure; thread-safe.
QualityScore ScoreQuality(const QualityMeasurement& m);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_QUALITY_SCORE_H_
