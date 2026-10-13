/*
 * File:        quality_assessor.h
 * Module:      quality
 * Purpose:     Assesses an encoding: its quality measures and overall
 *              score, behind an interface, and the score as a quality
 *              metric for Optimise
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_QUALITY_ASSESSOR_H_
#define SPEAK5220_CORE_QUALITY_QUALITY_ASSESSOR_H_

#include <span>

#include "core/encoder/encoder.h"
#include "core/lpc/frame.h"
#include "core/quality/lpc_envelope.h"
#include "core/quality/pitch_estimator.h"
#include "core/quality/quality_measures.h"
#include "core/quality/quality_metric.h"
#include "core/quality/quality_score.h"
#include "core/quality/voice_activity.h"
#include "core/synth/synth.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// The version of the measures and their calibration. A project's cached
// quality records it, and is measured again when it differs (plan §4), so
// it changes whenever the same encoding can be given other figures or
// another score.
inline constexpr int kMeasuresVersion = 1;

// An encoding's measures and the score made of them.
struct QualityAssessment {
  QualityMeasurement measures;
  QualityScore score;
};

// Assesses encodings (plan §3.3). Implementations are thread-safe.
class IQualityAssessor {
 public:
  virtual ~IQualityAssessor() = default;

  // Assesses `frames` (ending in a stop), made from `source` (8 kHz,
  // -1..1, as given to the encoder, gated when the encoder gated it).
  // Fails if the frames cannot be spoken, the source holds no speech, or a
  // measure cannot be taken.
  virtual StatusOr<QualityAssessment> Assess(
      std::span<const double> source, std::span<const lpc::Frame> frames) = 0;
};

// IQualityAssessor over MeasureQuality() and ScoreQuality(), with libfvad
// and SPTK's LPC analysis and REAPER. About 0.05 s per second of speech.
// Thread-safe if the synth is.
class QualityAssessor : public IQualityAssessor {
 public:
  // `synth` must outlive the assessor.
  explicit QualityAssessor(synth::ISynth& synth);

  StatusOr<QualityAssessment> Assess(
      std::span<const double> source,
      std::span<const lpc::Frame> frames) override;

 private:
  synth::ISynth* synth_;
  FvadVoiceActivity vad_;
  SptkLpcAnalysis lpc_;
  SptkPitchEstimator pitch_;
};

// IQualityMetric over an assessor: the overall score, 0 to 1, higher
// better. What Optimise maximises. Thread-safe if the assessor is.
class OverallQualityMetric : public IQualityMetric {
 public:
  // `assessor` must outlive the metric.
  explicit OverallQualityMetric(IQualityAssessor& assessor);

  StatusOr<double> Measure(std::span<const double> source,
                           const encoder::EncodeResult& encoding) override;
  MetricDirection Direction() const override {
    return MetricDirection::kHigherIsBetter;
  }

 private:
  IQualityAssessor* assessor_;
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_QUALITY_ASSESSOR_H_
