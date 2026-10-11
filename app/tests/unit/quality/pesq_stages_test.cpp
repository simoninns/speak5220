/*
 * File:        pesq_stages_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of PESQ's stages on synthetic signals: filtering,
 *              level and time alignment, the auditory transform and the
 *              aggregation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pesq_stages.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "core/quality/pesq_tables.h"
#include "gtest/gtest.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality::pesq {
namespace {

namespace t = pesq_tables;

constexpr std::size_t kBands = t::kBands;

std::vector<float> Floats(const std::vector<double>& x) {
  return {x.begin(), x.end()};
}

std::vector<float> Sine(double hz, int n, double amplitude) {
  std::vector<float> x(static_cast<std::size_t>(n));
  for (int i = 0; i < n; ++i) {
    x[static_cast<std::size_t>(i)] = static_cast<float>(
        amplitude * std::sin(2 * std::numbers::pi * hz * i / 8000.0));
  }
  return x;
}

// The RMS of data[from, to).
double Rms(const std::vector<float>& data, int from, int to) {
  double sum = 0.0;
  for (int i = from; i < to; ++i) {
    sum +=
        data[static_cast<std::size_t>(i)] * data[static_cast<std::size_t>(i)];
  }
  return std::sqrt(sum / (to - from));
}

// The preparation ScorePesq() gives a pair before time alignment.
struct Prepared {
  Signal ref;
  Signal deg;
  Scratch scratch;
};

Prepared Prepare(const std::vector<double>& ref,
                 const std::vector<double>& deg) {
  Prepared p{.ref = MakeSignal(Floats(ref)),
             .deg = MakeSignal(Floats(deg)),
             .scratch = {}};
  p.scratch = MakeScratch(p.ref, p.deg);
  const int max_samples = std::max(p.ref.samples, p.deg.samples);
  for (Signal* s : {&p.ref, &p.deg}) {
    FixPowerLevel(*s, max_samples);
    ApplyFilterCurve(s->data, s->samples, t::kIrsReceiveCurve);
    DcBlock(s->data, s->samples);
    ApplyInputFilter(s->data, s->samples + t::kDataPaddingSamples);
    ComputeVoiceActivity(*s);
  }
  return p;
}

TEST(PesqStagesTest, SignalsArePaddedWithSearchBuffers) {
  const Signal s = MakeSignal(std::vector<float>(4000, 1.0F));
  EXPECT_EQ(s.samples, 4000 + 2 * t::kSearchBufferSamples);
  EXPECT_EQ(s.data.size(),
            static_cast<std::size_t>(s.samples + t::kDataPaddingSamples));
  EXPECT_EQ(s.data[t::kSearchBufferSamples - 1], 0.0F);
  EXPECT_EQ(s.data[t::kSearchBufferSamples], 1.0F);
  EXPECT_EQ(s.data[t::kSearchBufferSamples + 4000], 0.0F);
  EXPECT_EQ(s.vad.size(), static_cast<std::size_t>(s.samples / 32));
}

TEST(PesqStagesTest, CurveInterpolatesBetweenAndBeyondItsPoints) {
  EXPECT_FLOAT_EQ(InterpolateCurve(1000.0F, t::kIrsReceiveCurve), 12.0F);
  EXPECT_FLOAT_EQ(InterpolateCurve(225.0F, t::kIrsReceiveCurve), 2.0F);
  EXPECT_FLOAT_EQ(InterpolateCurve(3375.0F, t::kIrsReceiveCurve), 8.0F);
  // Extrapolated from the first and last pairs of points.
  EXPECT_FLOAT_EQ(InterpolateCurve(-50.0F, t::kIrsReceiveCurve), -360.0F);
  EXPECT_FLOAT_EQ(InterpolateCurve(9000.0F, t::kIrsReceiveCurve), -200.0F);
}

TEST(PesqStagesTest, LevelAlignmentFilterPassesSpeechBandOnly) {
  for (const double hz : {1000.0, 100.0}) {
    Signal s = MakeSignal(Sine(hz, 16000, 1000.0));
    const double before = Rms(s.data, 6000, 14000);
    ApplyFilterCurve(s.data, s.samples, t::kLevelAlignCurve);
    const double after = Rms(s.data, 6000, 14000);
    if (hz == 1000.0) {
      EXPECT_NEAR(after / before, 1.0, 1e-2);
    } else {
      EXPECT_LT(after / before, 1e-2);
    }
  }
}

TEST(PesqStagesTest, PowerOfSumsSquaresOverTheDivisor) {
  const std::vector<float> x = {1, 2, 3, 4};
  EXPECT_DOUBLE_EQ(PowerOf(x, 1, 3, 4), (4.0 + 9.0) / 4.0);
}

TEST(PesqStagesTest, FixPowerLevelScalesToTheTargetPower) {
  Signal s = MakeSignal(Sine(1000.0, 16000, 0.01));
  FixPowerLevel(s, s.samples);
  std::vector<float> filtered = s.data;
  ApplyFilterCurve(filtered, s.samples, t::kLevelAlignCurve);
  const double power = PowerOf(
      filtered, t::kSearchBufferSamples,
      s.samples - t::kSearchBufferSamples + t::kDataPaddingSamples,
      s.samples - (2 * t::kSearchBufferSamples) + t::kDataPaddingSamples);
  EXPECT_NEAR(power / t::kTargetAveragePower, 1.0, 1e-3);
}

TEST(PesqStagesTest, DcBlockRemovesTheMeanAndTapersTheEnds) {
  Signal s = MakeSignal(std::vector<float>(8000, 5.0F));
  DcBlock(s.data, s.samples);
  // The mean is taken over the padded length, as the reference does, so a
  // little offset remains.
  const float middle = s.data[t::kSearchBufferSamples + 4000];
  EXPECT_NEAR(middle, 5.0F - 5.0F * 8000.0F / static_cast<float>(s.samples),
              1e-3F);
  EXPECT_NEAR(s.data[t::kSearchBufferSamples], middle * 0.5F / 32.0F, 1e-4F);
}

TEST(PesqStagesTest, InputFilterBlocksDirectCurrent) {
  std::vector<float> x(8000, 1.0F);
  ApplyInputFilter(x, static_cast<int>(x.size()));
  EXPECT_LT(std::fabs(x.back()), 1e-3F);
  std::vector<float> tone = Sine(1000.0, 8000, 1.0);
  ApplyInputFilter(tone, static_cast<int>(tone.size()));
  const double gain = Rms(tone, 4000, 8000) / std::sqrt(0.5);
  EXPECT_GT(gain, 0.1);
}

// The envelope frame of a signal sample.
std::size_t FrameOf(int sample) {
  return static_cast<std::size_t>((sample + t::kSearchBufferSamples) / 32);
}

TEST(PesqStagesTest, VoiceActivityFindsTheBurstsAndNotThePauses) {
  const Prepared p =
      Prepare(testing::SyntheticSpeech(), testing::SyntheticSpeech());
  EXPECT_GT(p.ref.vad[FrameOf(4000)], 0.0F);
  EXPECT_GT(p.ref.log_vad[FrameOf(4000)], 0.0F);
  EXPECT_GT(p.ref.vad[FrameOf(11600)], 0.0F);
  EXPECT_EQ(p.ref.vad[FrameOf(400)], 0.0F);
  EXPECT_EQ(p.ref.vad[FrameOf(15800)], 0.0F);
  EXPECT_EQ(p.ref.log_vad[FrameOf(400)], 0.0F);
}

TEST(PesqStagesTest, TimeAlignmentFindsAConstantDelayToTheSample) {
  constexpr int kDelay = 203;
  const std::vector<double> ref = testing::SyntheticSpeech();
  Prepared p = Prepare(ref, testing::Delayed(ref, kDelay));
  Alignment a;
  CrudeAlign(p.ref, p.deg, a, kWholeSignal, p.scratch);
  EXPECT_EQ(a.crude_delay % 32, 0);
  EXPECT_LE(std::abs(a.crude_delay - kDelay), 32);
  ASSERT_TRUE(LocateUtterances(p.ref, p.deg, a, p.scratch).IsOk());
  ASSERT_GE(a.utterances, 1);
  for (int u = 0; u < a.utterances; ++u) {
    EXPECT_EQ(a.delay[static_cast<std::size_t>(u)], kDelay) << u;
    EXPECT_GT(a.delay_confidence[static_cast<std::size_t>(u)], 0.0F);
  }
}

// A burst shorter than an utterance (kMinUtteranceLength frames, 200 ms)
// gives nothing to align. (With no burst at all, the voice activity
// detector takes the whole signal as speech.)
TEST(PesqStagesTest, NoUtteranceInABriefBurst) {
  std::vector<double> quiet(16000, 0.0);
  const std::vector<float> tone = Sine(500.0, 480, 0.5);
  for (std::size_t i = 0; i < tone.size(); ++i) {
    quiet[8000 + i] = tone[i];
  }
  Prepared p = Prepare(quiet, quiet);
  Alignment a;
  CrudeAlign(p.ref, p.deg, a, kWholeSignal, p.scratch);
  EXPECT_FALSE(LocateUtterances(p.ref, p.deg, a, p.scratch).IsOk());
}

TEST(PesqStagesTest, FrequencyWarpSumsBinsIntoBands) {
  std::vector<float> spectrum(t::kFrameSize / 2, 1.0F);
  std::array<float, kBands> density{};
  FrequencyWarp(spectrum, density);
  const auto sp = static_cast<double>(static_cast<float>(t::kPowerScale));
  for (std::size_t b = 0; b < kBands; ++b) {
    EXPECT_FLOAT_EQ(density[b],
                    static_cast<float>(t::kBinsPerBand[b] *
                                       t::kPowerDensityCorrection[b] * sp))
        << b;
  }
}

TEST(PesqStagesTest, TotalAudibleCountsBandsAboveTheThresholdFromBandOne) {
  std::array<float, kBands> density{};
  for (std::size_t b = 0; b < kBands; ++b) {
    density[b] = static_cast<float>(t::kAbsoluteThreshold[b]);
  }
  EXPECT_EQ(TotalAudible(density, 1.0F), 0.0F);
  density[0] = 1e9F;  // band 0 is never counted
  density[10] = 1000.0F;
  EXPECT_FLOAT_EQ(TotalAudible(density, 1.0F), 1000.0F);
  EXPECT_EQ(TotalAudible(density, 1000.0F), 0.0F);
}

TEST(PesqStagesTest, LoudnessIsZeroAtThresholdAndGrowsAboveIt) {
  std::array<float, kBands> quiet{};
  std::array<float, kBands> loud{};
  std::array<float, kBands> louder{};
  for (std::size_t b = 0; b < kBands; ++b) {
    quiet[b] = static_cast<float>(t::kAbsoluteThreshold[b]);
    loud[b] = static_cast<float>(t::kAbsoluteThreshold[b] * 100);
    louder[b] = static_cast<float>(t::kAbsoluteThreshold[b] * 10000);
  }
  std::array<float, kBands> lq{};
  std::array<float, kBands> ll{};
  std::array<float, kBands> lr{};
  IntensityWarp(quiet, lq);
  IntensityWarp(loud, ll);
  IntensityWarp(louder, lr);
  for (std::size_t b = 0; b < kBands; ++b) {
    EXPECT_EQ(lq[b], 0.0F) << b;
    EXPECT_GT(ll[b], 0.0F) << b;
    EXPECT_GT(lr[b], ll[b]) << b;
  }
}

TEST(PesqStagesTest, PseudoLpOfAConstantIsItTimesTheTotalWidth) {
  std::array<float, kBands> x{};
  x.fill(-2.0F);
  double width = 0.0;
  double width_squared = 0.0;
  for (std::size_t b = 1; b < kBands; ++b) {
    const double w = static_cast<float>(t::kBandWidthBark[b]);
    width += w;
    width_squared += w * w;
  }
  EXPECT_NEAR(PseudoLp(x, 1.0F), 2.0 * width, 1e-4);
  // (sum (2 w)^2 / W)^(1/2) W.
  EXPECT_NEAR(PseudoLp(x, 2.0F), 2.0 * std::sqrt(width_squared / width) * width,
              1e-4);
}

TEST(PesqStagesTest, AsymmetryKeepsOnlyAddedComponents) {
  std::array<float, kBands> disturbance{};
  disturbance.fill(1.0F);
  std::array<float, kBands> ref{};
  std::array<float, kBands> deg{};
  ref.fill(100.0F);
  deg.fill(100.0F);
  deg[1] = 1e6F;    // far louder: clipped at 12
  deg[2] = 300.0F;  // (350 / 150)^1.2 = 2.76: below 3, zero
  deg[3] = 400.0F;  // (450 / 150)^1.2 = 3.74
  MultiplyByAsymmetry(disturbance, ref, deg);
  EXPECT_EQ(disturbance[0], 0.0F);
  EXPECT_EQ(disturbance[1], 12.0F);
  EXPECT_EQ(disturbance[2], 0.0F);
  EXPECT_NEAR(disturbance[3], std::pow(3.0, 1.2), 1e-5);
}

TEST(PesqStagesTest, LpqWeightOfAConstantIsTheConstant) {
  const std::vector<float> d(100, 3.0F);
  const std::vector<float> w(100, 1.0F);
  // Frames past the stop frame count as zero in the last intervals.
  EXPECT_NEAR(LpqWeight(0, 99, 6.0F, 2.0F, d, w), 3.0, 0.3);
  EXPECT_NEAR(LpqWeight(0, 99, 1.0F, 1.0F, d, w), 3.0 * (190.0 / 200.0), 1e-5);
}

TEST(PesqStagesTest, ComputeDelayFindsAShift) {
  const std::vector<double> noise = testing::WhiteNoise(2048, 1000.0);
  std::vector<float> x1(4096, 0.0F);
  std::vector<float> x2(4096, 0.0F);
  for (std::size_t i = 0; i < noise.size(); ++i) {
    x1[1000 + i] = static_cast<float>(noise[i]);
    x2[1037 + i] = static_cast<float>(noise[i]);
  }
  const DelayEstimate e = ComputeDelay(0, 4096, 512, x1, x2);
  EXPECT_EQ(e.delay, 37);
  EXPECT_GT(e.correlation, 0.5F);
  const std::vector<float> silent(4096, 0.0F);
  EXPECT_EQ(ComputeDelay(0, 4096, 512, x1, silent).correlation, 0.0F);
}

}  // namespace
}  // namespace speak5220::core::quality::pesq
