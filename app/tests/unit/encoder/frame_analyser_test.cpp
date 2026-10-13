/*
 * File:        frame_analyser_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the frame analysis: the level measure, the
 *              speech segmentation and the frame features
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/frame_analyser.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/encoder/encoder_filters.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

// Silence, then a tone of `hz` (300 Hz by default) from sample `start` for
// `length` samples, then silence, `total` samples in all.
std::vector<double> ToneBurst(int start, int length, int total,
                              double hz = 300.0) {
  std::vector<double> x(static_cast<std::size_t>(total), 0.0);
  for (int n = start; n < start + length && n < total; ++n) {
    x[static_cast<std::size_t>(n)] =
        0.3 * std::sin(2.0 * std::numbers::pi * hz * n / 8000.0);
  }
  return x;
}

// The middle frame's features of a tone burst of `hz`, all frames voiced.
FrameFeatures MiddleFrameOfTone(double hz) {
  FrameAnalyser analyser;
  const EncoderOptions options;
  const auto seg = analyser.Segment(ToneBurst(2000, 4000, 8000, hz), options);
  EXPECT_TRUE(seg.has_value());
  if (!seg.has_value()) {
    return {};
  }
  const std::vector<FrameVoicing> voicing(seg->centres.size(),
                                          {.voiced = true, .period = 80.0});
  const auto feats = analyser.Features(*seg, voicing, options);
  return feats[feats.size() / 2];
}

// |H|^2 of LoudnessHighPass() at a multiple of 50 Hz.
double LoudnessPowerAt(int hz) {
  constexpr int kStepHz = 50;
  const std::vector<double> power = LoudnessHighPassPower(81);
  return power[static_cast<std::size_t>(hz / kStepHz)];
}

TEST(FrameAnalyserTest, FrameRmsOfConstantIsTheConstant) {
  const std::vector<double> x(1000, 0.5);
  EXPECT_NEAR(FrameRms(x, 500), 0.5, 1e-12);
  EXPECT_NEAR(FrameRms(x, 500, 40), 0.5, 1e-12);
  EXPECT_NEAR(FrameRms(x, 500, 7), 0.5, 1e-12);
}

TEST(FrameAnalyserTest, FrameRmsKeepsTheFullWeightAtTheEdges) {
  const std::vector<double> x(1000, 0.5);
  // Centred on sample 0, half the window is outside: about half the
  // power.
  const std::vector<double> w = dsp::InnerHannWindow(201);
  double inside = 0.0;
  for (std::size_t i = 100; i < w.size(); ++i) {
    inside += w[i];
  }
  EXPECT_NEAR(FrameRms(x, 0), 0.5 * std::sqrt(inside / dsp::PairwiseSum(w)),
              1e-12);
  EXPECT_EQ(FrameRms(x, -500), 0.0);
}

TEST(FrameAnalyserTest, SilenceHasNoSpeech) {
  FrameAnalyser analyser;
  EXPECT_FALSE(
      analyser.Segment(std::vector<double>(4000, 0.0), {}).has_value());
}

TEST(FrameAnalyserTest, FindsToneBurstAndPlacesFramesFromItsOnset) {
  FrameAnalyser analyser;
  const auto seg = analyser.Segment(ToneBurst(2000, 4000, 8000), {});
  ASSERT_TRUE(seg.has_value());
  EXPECT_NEAR(seg->onset, 2000, 60);
  EXPECT_NEAR(seg->offset, 6000, 60);
  EXPECT_EQ(seg->c0, seg->onset + 22);
  ASSERT_FALSE(seg->centres.empty());
  EXPECT_EQ(seg->centres.front(), seg->c0);
  for (std::size_t j = 1; j < seg->centres.size(); ++j) {
    EXPECT_EQ(seg->centres[j] - seg->centres[j - 1], kFrameSamples);
  }
  // The frames cover the burst up to its offset.
  EXPECT_GE(seg->centres.back() + kFrameSamples, seg->offset);
  EXPECT_EQ(seg->x.size(), 8000U);
}

TEST(FrameAnalyserTest, FeaturesCarryVoicingLevelAndGate) {
  FrameAnalyser analyser;
  const EncoderOptions options;
  const auto seg = analyser.Segment(ToneBurst(2000, 4000, 8000), options);
  ASSERT_TRUE(seg.has_value());
  std::vector<FrameVoicing> voicing(seg->centres.size(),
                                    {.voiced = true, .period = 80.0});
  voicing.back() = {.voiced = false, .period = 0.0};
  const auto feats = analyser.Features(*seg, voicing, options);
  ASSERT_EQ(feats.size(), seg->centres.size());
  const FrameFeatures& mid = feats[feats.size() / 2];
  EXPECT_TRUE(mid.voiced);
  EXPECT_EQ(mid.period, 80.0);
  EXPECT_FALSE(mid.gated);
  // The tone's RMS is 0.3 / sqrt(2).
  EXPECT_NEAR(mid.level, 0.3 / std::numbers::sqrt2, 0.01);
  EXPECT_GT(mid.rv[0], 0.0);
  EXPECT_GT(mid.ru[0], 0.0);
  EXPECT_FALSE(feats.back().voiced);
}

// Of two tones of equal RMS, the 100 Hz tone's loudness is about 9 dB
// below its level (itself 1 dB down after the 60 Hz high-pass), the 1 kHz
// tone's barely below: bass does not count towards the level aimed at.
TEST(FrameAnalyserTest, LoudnessIsTheLevelAfterTheLoudnessHighPass) {
  const FrameFeatures low = MiddleFrameOfTone(100.0);
  const FrameFeatures high = MiddleFrameOfTone(1000.0);
  EXPECT_NEAR(low.loudness / low.level, std::sqrt(LoudnessPowerAt(100)), 0.01);
  EXPECT_NEAR(high.loudness / high.level, std::sqrt(LoudnessPowerAt(1000)),
              0.01);
  EXPECT_LT(20.0 * std::log10(low.loudness / high.loudness), -8.0);
}

TEST(FrameAnalyserTest, LoudnessHighPassIsHalfPowerAtItsCorner) {
  // The bilinear transform is prewarped, so the corner stays at 279 Hz.
  const std::vector<double> power = LoudnessHighPassPower(4001);
  EXPECT_NEAR(power[static_cast<std::size_t>(kLoudnessHighPassHz)], 0.5, 0.001);
  EXPECT_EQ(power.front(), 0.0);
  EXPECT_NEAR(power.back(), 1.0, 1e-12);
  // The filter itself removes DC.
  const std::vector<double> dc =
      LoudnessHighPass(std::vector<double>(4000, 1.0));
  EXPECT_LT(std::fabs(dc.back()), 1e-6);
}

TEST(FrameAnalyserTest, AutocorrelationGetsItsNoiseFloor) {
  FrameAnalyser analyser;
  EncoderOptions options;
  options.lag_window_hz = 0.0;  // no lag window: only the floor changes r
  const auto seg = analyser.Segment(ToneBurst(2000, 4000, 8000), options);
  ASSERT_TRUE(seg.has_value());
  const std::vector<FrameVoicing> voicing(seg->centres.size());
  const auto feats = analyser.Features(*seg, voicing, options);
  const FrameFeatures& mid = feats[feats.size() / 2];
  // A 300 Hz tone: r[1] / r[0] is close to cos(2 pi 300 / 8000), less
  // the 1.0001 floor on r[0].
  EXPECT_LT(mid.ru[1] / mid.ru[0], 1.0 / 1.0001);
  EXPECT_NEAR(mid.ru[1] / mid.ru[0],
              std::cos(2.0 * std::numbers::pi * 300.0 / 8000.0), 0.01);
}

}  // namespace
}  // namespace speak5220::core::encoder
