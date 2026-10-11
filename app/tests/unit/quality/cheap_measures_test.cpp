/*
 * File:        cheap_measures_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the cheap level and spectral measures
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/cheap_measures.h"

#include <cmath>
#include <memory>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/lpc/frame.h"
#include "gtest/gtest.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

encoder::EncodeResult EncodeSyntheticSpeech() {
  const std::unique_ptr<encoder::IEncoder> encoder = encoder::MakeEncoder();
  const auto result = encoder->Encode(testing::SyntheticSpeech(), 8000,
                                      encoder::EncoderOptions{});
  EXPECT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  return result.IsOk() ? *result : encoder::EncodeResult{};
}

TEST(CheapMeasuresTest, AnEncodingFollowsItsInputClosely) {
  const encoder::EncodeResult e = EncodeSyntheticSpeech();
  ASSERT_TRUE(e.details.has_value());
  const CheapMeasures m = MeasureCheap(*e.details, e.frames);
  EXPECT_GE(m.level_error, 0.0);
  EXPECT_LT(m.level_error, 6.0);
  EXPECT_GE(m.level_p90, 0.0);
  EXPECT_LT(std::fabs(m.level_bias), 6.0);
  EXPECT_GT(m.spectral_distance, 0.0);
  EXPECT_LT(m.spectral_distance, 10.0);
}

TEST(CheapMeasuresTest, SilentPlanHasNoFramesToCompare) {
  encoder::EncodeDetails details;
  details.x.assign(4000, 0.0);
  details.gain = 1.0;
  details.centres = {100, 300};
  details.plan.resize(2);  // silence
  const std::vector<lpc::Frame> frames = {lpc::Frame::Silence(),
                                          lpc::Frame::Stop()};
  const CheapMeasures m = MeasureCheap(details, frames);
  EXPECT_EQ(m.level_error, 0.0);
  EXPECT_EQ(m.level_bias, 0.0);
  EXPECT_TRUE(std::isnan(m.spectral_distance));
}

TEST(CheapMeasuresTest, MetricsMeasureTheEncodingAndNeedItsAnalysis) {
  const encoder::EncodeResult e = EncodeSyntheticSpeech();
  ASSERT_TRUE(e.details.has_value());
  const CheapMeasures m = MeasureCheap(*e.details, e.frames);
  LevelErrorMetric level;
  SpectralDistanceMetric spectrum;
  const std::vector<double> source = testing::SyntheticSpeech();
  EXPECT_EQ(*level.Measure(source, e), m.level_error);
  EXPECT_EQ(*spectrum.Measure(source, e), m.spectral_distance);
  EXPECT_EQ(level.Direction(), MetricDirection::kLowerIsBetter);
  EXPECT_EQ(spectrum.Direction(), MetricDirection::kLowerIsBetter);
  encoder::EncodeResult no_sound;
  no_sound.frames = {lpc::Frame::Stop()};
  EXPECT_FALSE(level.Measure(source, no_sound).IsOk());
  EXPECT_FALSE(spectrum.Measure(source, no_sound).IsOk());
}

}  // namespace
}  // namespace speak5220::core::quality
