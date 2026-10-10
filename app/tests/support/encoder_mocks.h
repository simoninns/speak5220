/*
 * File:        encoder_mocks.h
 * Module:      test support
 * Purpose:     gMock mocks of the encoder's stage interfaces and the FFT
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_ENCODER_MOCKS_H_
#define SPEAK5220_TESTS_SUPPORT_ENCODER_MOCKS_H_

#include <complex>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_analyser.h"
#include "core/encoder/level_setter.h"
#include "core/encoder/pitch_tracker.h"
#include "core/encoder/repeat_search.h"
#include "core/encoder/voicing_classifier.h"
#include "gmock/gmock.h"

namespace speak5220::testing {

class MockFft : public core::dsp::IFft {
 public:
  MOCK_METHOD(std::vector<std::complex<double>>, Forward,
              (std::span<const double> input), (override));
  MOCK_METHOD(std::vector<double>, Inverse,
              (std::span<const std::complex<double>> bins, std::size_t n),
              (override));
};

class MockFrameAnalyser : public core::encoder::IFrameAnalyser {
 public:
  MOCK_METHOD(std::optional<core::encoder::Segmentation>, Segment,
              (std::span<const double> samples,
               const core::encoder::EncoderOptions& options),
              (override));
  MOCK_METHOD(std::vector<core::encoder::FrameFeatures>, Features,
              (const core::encoder::Segmentation& segmentation,
               std::span<const core::encoder::FrameVoicing> voicing,
               const core::encoder::EncoderOptions& options),
              (override));
};

class MockPitchTracker : public core::encoder::IPitchTracker {
 public:
  MOCK_METHOD(core::encoder::PitchTrack, Track,
              (std::span<const double> x,
               const core::encoder::EncoderOptions& options),
              (override));
};

class MockVoicingClassifier : public core::encoder::IVoicingClassifier {
 public:
  MOCK_METHOD(std::vector<core::encoder::FrameVoicing>, Classify,
              (const core::encoder::PitchTrack& track,
               std::span<const int> centres),
              (override));
};

class MockRepeatSearch : public core::encoder::IRepeatSearch {
 public:
  MOCK_METHOD(std::vector<core::encoder::PlanEntry>, Plan,
              (std::span<const core::encoder::FrameFeatures> features,
               const core::encoder::EncoderOptions& options),
              (override));
};

class MockLevelSetter : public core::encoder::ILevelSetter {
 public:
  MOCK_METHOD(std::vector<double>, ShapeLevels,
              (std::span<const double> levels,
               std::span<const core::encoder::PlanEntry> plan,
               const core::encoder::EncoderOptions& options),
              (override));
  MOCK_METHOD(std::vector<int>, SetLevels,
              (std::span<const core::encoder::PlanEntry> plan,
               std::span<const double> levels, double gain,
               const core::encoder::EncoderOptions& options,
               core::encoder::LevelStats* stats),
              (override));
};

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_ENCODER_MOCKS_H_
