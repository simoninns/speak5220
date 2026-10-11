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
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

// Silence, then a 300 Hz tone from sample `start` for `length` samples,
// then silence, `total` samples in all.
std::vector<double> ToneBurst(int start, int length, int total) {
  std::vector<double> x(static_cast<std::size_t>(total), 0.0);
  for (int n = start; n < start + length && n < total; ++n) {
    x[static_cast<std::size_t>(n)] =
        0.3 * std::sin(2.0 * std::numbers::pi * 300.0 * n / 8000.0);
  }
  return x;
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
