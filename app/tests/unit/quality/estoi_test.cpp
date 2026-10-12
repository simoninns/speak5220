/*
 * File:        estoi_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the ESTOI port, stage by stage: the
 *              one-third-octave bands, the resampler, the removal of silent
 *              frames, the segment normalisation and the whole measure
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/estoi.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

#include "core/quality/signal_preparation.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

std::vector<double> Tone(double hz, double rate, std::size_t n) {
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i) {
    x[i] = 0.5 * std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) /
                          rate);
  }
  return x;
}

TEST(EstoiTest, ThirdOctaveBandsSpanTheirEdgesNearestBins) {
  const auto bands = ThirdOctaveBands();
  // Band 0: 133.6 to 168.4 Hz, bins of 19.53 Hz: 7 (136.7) to 9 (175.8).
  EXPECT_EQ(bands.front().first, 7);
  EXPECT_EQ(bands.front().end, 9);
  // Band 14: 3394 to 4276 Hz: bins 174 to 219.
  EXPECT_EQ(bands.back().first, 174);
  EXPECT_EQ(bands.back().end, 219);
  for (std::size_t b = 1; b < bands.size(); ++b) {
    EXPECT_EQ(bands[b].first, bands[b - 1].end) << b;
  }
}

TEST(EstoiTest, ResamplerKeepsAToneAndGivesTheLength) {
  const std::vector<double> x = Tone(1000.0, 8000.0, 8000);
  const std::vector<double> y = ResampleLikeOctave(x, 8000, 10000);
  ASSERT_EQ(y.size(), 10000U);
  // Away from the ends, the tone at 10 kHz.
  double worst = 0.0;
  for (std::size_t i = 1000; i < 9000; ++i) {
    const double expected = 0.5 * std::sin(2.0 * std::numbers::pi * 1000.0 *
                                           static_cast<double>(i) / 10000.0);
    worst = std::max(worst, std::fabs(y[i] - expected));
  }
  EXPECT_LT(worst, 0.005);
  EXPECT_EQ(ResampleLikeOctave(x, 8000, 8000), x);
}

TEST(EstoiTest, RemovingSilentFramesKeepsOnlyTheSound) {
  // 20 loud frames' worth, then silence as long.
  std::vector<double> x = Tone(500.0, 10000.0, std::size_t{20} * kEstoiHop);
  x.resize(std::size_t{40} * kEstoiHop, 0.0);
  const std::vector<double> y(x.size(), 0.25);
  const auto [x_kept, y_kept] = RemoveSilentFrames(x, y);
  ASSERT_EQ(x_kept.size(), y_kept.size());
  // Frames starting at 0..18 hops lie wholly in the tone, 19 overlaps it
  // by half; the rest start in the silence.
  const std::size_t frames = (x_kept.size() - kEstoiFrame) / kEstoiHop + 1;
  EXPECT_EQ(frames, 20U);
  // Inside, the Hann frames at half overlap sum to the signal.
  EXPECT_NEAR(y_kept[std::size_t{5} * kEstoiHop], 0.25, 0.01);
}

TEST(EstoiTest, RemovingSilentFramesOfSilenceLeavesNothing) {
  const std::vector<double> x(std::size_t{10} * kEstoiHop, 0.0);
  // Every frame is the loudest; all are kept.
  EXPECT_FALSE(RemoveSilentFrames(x, x).first.empty());
  EXPECT_TRUE(
      RemoveSilentFrames(std::vector<double>(10), std::vector<double>(10))
          .first.empty());
}

// Envelopes of kEstoiBands rows that vary over time and across bands.
std::vector<std::vector<double>> Envelopes(double scale, double offset) {
  std::vector<std::vector<double>> m(kEstoiBands,
                                     std::vector<double>(kEstoiSegment + 5));
  for (std::size_t b = 0; b < m.size(); ++b) {
    for (std::size_t t = 0; t < m[b].size(); ++t) {
      m[b][t] =
          offset +
          scale * (1.0 + std::sin(0.3 * static_cast<double>(t * (b + 1))));
    }
  }
  return m;
}

TEST(EstoiTest, IdenticalSegmentsCorrelateFully) {
  const auto x = Envelopes(1.0, 0.0);
  EXPECT_NEAR(SegmentIntelligibility(x, x, 2), 1.0, 1e-12);
  // Each band normalised: scale and offset do not matter.
  EXPECT_NEAR(SegmentIntelligibility(x, Envelopes(7.0, 3.0), 2), 1.0, 1e-12);
}

TEST(EstoiTest, InvertedSegmentsAnticorrelate) {
  const auto x = Envelopes(1.0, 0.0);
  EXPECT_NEAR(SegmentIntelligibility(x, Envelopes(-1.0, 0.0), 0), -1.0, 1e-12);
}

TEST(EstoiTest, FlatSegmentGivesZeroNotNan) {
  const auto x = Envelopes(1.0, 0.0);
  const std::vector<std::vector<double>> flat(
      kEstoiBands, std::vector<double>(kEstoiSegment, 2.0));
  const double d = SegmentIntelligibility(x, flat, 0);
  EXPECT_FALSE(std::isnan(d));
  EXPECT_EQ(d, 0.0);
}

TEST(EstoiTest, ASignalAgainstItselfScoresOne) {
  const std::vector<double> x = testing::SyntheticSpeech();
  const StatusOr<double> d = Estoi(x, x);
  ASSERT_TRUE(d.IsOk()) << d.GetStatus().ToString();
  EXPECT_NEAR(*d, 1.0, 1e-9);
}

TEST(EstoiTest, NoiseScoresFarBelowTheSignal) {
  const std::vector<double> x = testing::SyntheticSpeech();
  std::vector<double> y(x.size());
  // A fixed pseudo-random sequence, deterministic.
  unsigned state = 12345U;
  for (double& v : y) {
    state = state * 1103515245U + 12345U;
    v = (static_cast<double>((state >> 16) & 0x7FFFU) / 32768.0 - 0.5) * 0.2;
  }
  const StatusOr<double> d = Estoi(x, y);
  ASSERT_TRUE(d.IsOk());
  EXPECT_LT(*d, 0.2);
}

TEST(EstoiTest, TooLittleSoundFails) {
  const std::vector<double> x = Tone(500.0, 8000.0, 2000);
  EXPECT_EQ(Estoi(x, x).GetStatus().Code(), StatusCode::kFailedPrecondition);
  EXPECT_EQ(Estoi(x, std::vector<double>(10)).GetStatus().Code(),
            StatusCode::kInvalidArgument);
}

TEST(EstoiTest, FramesOutsideTheMaskAreLeftOut) {
  // The output matches the original inside the mask and is noise outside
  // it; with the mask, it scores as if identical.
  const std::vector<double> x = testing::SyntheticSpeech();
  PreparedSignals p;
  p.original = x;
  p.output = x;
  p.speech.assign(x.size() / kVoiceActivityFrame, true);
  const std::size_t half = p.speech.size() / 2;
  for (std::size_t f = half; f < p.speech.size(); ++f) {
    p.speech[f] = false;
  }
  for (std::size_t i = half * kVoiceActivityFrame; i < x.size(); ++i) {
    p.output[i] = (i % 7 == 0) ? 0.3 : -0.05;
  }
  const StatusOr<double> d = MeasureIntelligibility(p);
  ASSERT_TRUE(d.IsOk()) << d.GetStatus().ToString();
  EXPECT_NEAR(*d, 1.0, 1e-9);
}

}  // namespace
}  // namespace speak5220::core::quality
