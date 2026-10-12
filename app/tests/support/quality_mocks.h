/*
 * File:        quality_mocks.h
 * Module:      test support
 * Purpose:     gMock mocks of the quality interfaces: a quality metric, the
 *              quality assessor, and the voice activity, LPC analysis and
 *              pitch estimators the measures use
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_QUALITY_MOCKS_H_
#define SPEAK5220_TESTS_SUPPORT_QUALITY_MOCKS_H_

#include <span>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/quality/lpc_envelope.h"
#include "core/quality/pitch_estimator.h"
#include "core/quality/quality_assessor.h"
#include "core/quality/quality_metric.h"
#include "core/quality/voice_activity.h"
#include "core/util/status.h"
#include "gmock/gmock.h"

namespace speak5220::testing {

class MockQualityMetric : public core::quality::IQualityMetric {
 public:
  MOCK_METHOD(core::StatusOr<double>, Measure,
              (std::span<const double> source,
               const core::encoder::EncodeResult& encoding),
              (override));
  MOCK_METHOD(core::quality::MetricDirection, Direction, (), (const, override));
};

class MockQualityAssessor : public core::quality::IQualityAssessor {
 public:
  MOCK_METHOD(core::StatusOr<core::quality::QualityAssessment>, Assess,
              (std::span<const double> source,
               std::span<const core::lpc::Frame> frames),
              (override));
};

// An assessment whose overall score is `overall`: every figure at the same
// point of its range, so each component is `overall` too.
core::quality::QualityAssessment AssessmentScoring(double overall);

class MockVoiceActivity : public core::quality::IVoiceActivity {
 public:
  MOCK_METHOD(core::StatusOr<std::vector<bool>>, SpeechFrames,
              (std::span<const double> samples), (override));
};

class MockLpcAnalysis : public core::quality::ILpcAnalysis {
 public:
  MOCK_METHOD(core::StatusOr<core::quality::LpcFit>, Analyse,
              (std::span<const double> frame, int order), (override));
  MOCK_METHOD(core::StatusOr<std::vector<double>>, EnvelopeDb,
              (const core::quality::LpcFit& fit, int fft_length), (override));
};

class MockPitchEstimator : public core::quality::IPitchEstimator {
 public:
  MOCK_METHOD(core::StatusOr<std::vector<double>>, Track,
              (std::span<const double> samples), (override));
};

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_QUALITY_MOCKS_H_
