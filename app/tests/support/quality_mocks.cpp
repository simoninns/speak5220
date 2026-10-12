/*
 * File:        quality_mocks.cpp
 * Module:      test support
 * Purpose:     Assessments with a chosen overall score, for tests that mock
 *              the quality assessor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/quality_mocks.h"

#include "core/quality/quality_assessor.h"
#include "core/quality/quality_score.h"

namespace speak5220::testing {

core::quality::QualityAssessment AssessmentScoring(double overall) {
  using C = core::quality::ScoreCalibration;
  const auto at = [overall](core::quality::ScoreRange r) {
    return r.poor + overall * (r.good - r.poor);
  };
  core::quality::QualityAssessment a;
  a.measures.envelope.mean_db = at(C::kLsd);
  a.measures.envelope.over_4db = at(C::kLargeOutliers);
  a.measures.pitch.gross_error = at(C::kGrossPitch);
  a.measures.pitch.f0_rmse_cents = at(C::kF0Rmse);
  a.measures.pitch.voicing_error = at(C::kVoicing);
  a.measures.estoi = at(C::kEstoi);
  a.measures.speech_frames = 1;
  a.score = core::quality::ScoreQuality(a.measures);
  return a;
}

}  // namespace speak5220::testing
