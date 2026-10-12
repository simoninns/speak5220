/*
 * File:        cached_quality.cpp
 * Module:      quality
 * Purpose:     Converts between an encoding's quality assessment and the
 *              figures a project caches with the encoding
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/cached_quality.h"

#include "core/quality/quality_score.h"

namespace speak5220::core::quality {

project::CachedQuality ToCachedQuality(const QualityAssessment& assessment) {
  const QualityMeasurement& m = assessment.measures;
  return {.measures_version = kMeasuresVersion,
          .lsd_db = m.envelope.mean_db,
          .lsd_over_2db = m.envelope.over_2db,
          .lsd_over_4db = m.envelope.over_4db,
          .gross_pitch_error = m.pitch.gross_error,
          .f0_rmse_cents = m.pitch.f0_rmse_cents,
          .voicing_error = m.pitch.voicing_error,
          .estoi = m.estoi,
          .score = assessment.score.overall};
}

QualityAssessment FromCachedQuality(const project::CachedQuality& cached) {
  QualityAssessment assessment;
  QualityMeasurement& m = assessment.measures;
  m.envelope.mean_db = cached.lsd_db;
  m.envelope.over_2db = cached.lsd_over_2db;
  m.envelope.over_4db = cached.lsd_over_4db;
  m.pitch.gross_error = cached.gross_pitch_error;
  m.pitch.f0_rmse_cents = cached.f0_rmse_cents;
  m.pitch.voicing_error = cached.voicing_error;
  m.estoi = cached.estoi;
  assessment.score = ScoreQuality(m);
  return assessment;
}

}  // namespace speak5220::core::quality
