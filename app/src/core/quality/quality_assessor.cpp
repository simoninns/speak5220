/*
 * File:        quality_assessor.cpp
 * Module:      quality
 * Purpose:     Assesses an encoding: its quality measures and overall
 *              score, behind an interface, and the score as a quality
 *              metric for Optimise
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/quality_assessor.h"

#include <span>

#include "core/quality/signal_preparation.h"
#include "core/util/logging.h"

namespace speak5220::core::quality {

QualityAssessor::QualityAssessor(synth::ISynth& synth)
    : synth_(&synth),
      vad_(kSpeechDetectorMode),
      pitch_(PitchAlgorithm::kReaper) {}

StatusOr<QualityAssessment> QualityAssessor::Assess(
    std::span<const double> source, std::span<const lpc::Frame> frames) {
  StatusOr<QualityMeasurement> measures = MeasureQuality(
      source, frames,
      {.synth = synth_, .vad = &vad_, .lpc = &lpc_, .pitch = &pitch_});
  if (!measures.IsOk()) {
    return measures.GetStatus();
  }
  QualityAssessment assessment{.measures = *measures,
                               .score = ScoreQuality(*measures)};
  Logger(kLogModuleQuality)
      ->debug("quality: {:.2f} ({})", assessment.score.overall,
              RatingLabel(assessment.score.rating));
  return assessment;
}

OverallQualityMetric::OverallQualityMetric(IQualityAssessor& assessor)
    : assessor_(&assessor) {}

StatusOr<double> OverallQualityMetric::Measure(
    std::span<const double> source, const encoder::EncodeResult& encoding) {
  StatusOr<QualityAssessment> assessment =
      assessor_->Assess(source, encoding.frames);
  if (!assessment.IsOk()) {
    return assessment.GetStatus();
  }
  return assessment->score.overall;
}

}  // namespace speak5220::core::quality
