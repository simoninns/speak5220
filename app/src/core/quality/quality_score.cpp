/*
 * File:        quality_score.cpp
 * Module:      quality
 * Purpose:     The overall quality score and rating: the measures mapped to
 *              component scores and combined by a weighted geometric mean
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/quality_score.h"

#include <algorithm>
#include <cmath>
#include <string_view>

namespace speak5220::core::quality {

using C = ScoreCalibration;

double MapToScore(double value, ScoreRange range) {
  const double t = (value - range.poor) / (range.good - range.poor);
  return std::clamp(t, 0.0, 1.0);
}

QualityComponents ScoreComponents(const QualityMeasurement& m) {
  QualityComponents c;
  c.envelope = (MapToScore(m.envelope.mean_db, C::kLsd) +
                MapToScore(m.envelope.over_4db, C::kLargeOutliers)) /
               2.0;
  c.pitch = (MapToScore(m.pitch.gross_error, C::kGrossPitch) +
             MapToScore(m.pitch.f0_rmse_cents, C::kF0Rmse) +
             MapToScore(m.pitch.voicing_error, C::kVoicing)) /
            3.0;
  if (m.estoi.has_value()) {
    c.intelligibility = MapToScore(*m.estoi, C::kEstoi);
  }
  return c;
}

double CombineComponents(const QualityComponents& components) {
  double envelope_weight = C::kEnvelopeWeight;
  double pitch_weight = C::kPitchWeight;
  double intelligibility_weight = C::kIntelligibilityWeight;
  if (!components.intelligibility.has_value()) {
    const double rest = envelope_weight + pitch_weight;
    envelope_weight /= rest;
    pitch_weight /= rest;
    intelligibility_weight = 0.0;
  }
  return std::pow(components.envelope, envelope_weight) *
         std::pow(components.pitch, pitch_weight) *
         std::pow(components.intelligibility.value_or(1.0),
                  intelligibility_weight);
}

QualityRating RatingOf(double overall) {
  if (overall >= C::kGoodFrom) {
    return QualityRating::kGood;
  }
  if (overall >= C::kFairFrom) {
    return QualityRating::kFair;
  }
  return QualityRating::kPoor;
}

std::string_view RatingLabel(QualityRating rating) {
  switch (rating) {
    case QualityRating::kPoor:
      return "Poor";
    case QualityRating::kFair:
      return "Fair";
    case QualityRating::kGood:
      return "Good";
  }
  return "Poor";
}

QualityScore ScoreQuality(const QualityMeasurement& m) {
  QualityScore score;
  score.components = ScoreComponents(m);
  score.overall = CombineComponents(score.components);
  score.rating = RatingOf(score.overall);
  return score;
}

}  // namespace speak5220::core::quality
