/*
 * File:        level_setter_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the open-loop gain, the output level measure,
 *              level shaping and the closed-loop energies
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/level_setter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_endings.h"
#include "core/lpc/coding_tables.h"
#include "core/synth/reference_synthesizer.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

constexpr double kTolerance = 1e-9;
const std::vector<int> kK10 = {20, 15, 8, 8, 8, 8, 8, 4, 4, 4};
const std::vector<int> kK4 = {20, 15, 8, 8};

PlanEntry Voiced(PlanKind kind = PlanKind::kVoiced) {
  return {.kind = kind, .k = kK10, .pitch = 30};
}

TEST(LevelSetterTest, GainOfFlatFilterWithNoiseIsHalf) {
  dsp::PocketFft fft;
  // RMS 64 excitation through a flat filter, / 128 (§12.3).
  EXPECT_NEAR(GainOf(fft, {}, 0), 0.5, kTolerance);
}

TEST(LevelSetterTest, GainOfFlatFilterWithChirpIsChirpEnergyPerPeriod) {
  dsp::PocketFft fft;
  double energy = 0.0;
  for (std::size_t i = 0; i < 21; ++i) {
    energy += static_cast<double>(lpc::kChirpTable[i] * lpc::kChirpTable[i]);
  }
  EXPECT_NEAR(GainOf(fft, {}, 100), std::sqrt(energy / 100.0) / 128.0,
              kTolerance);
}

TEST(LevelSetterTest, EffectiveKFollowsTheLastFullFrame) {
  const std::vector<PlanEntry> plan = {
      Voiced(),
      Voiced(PlanKind::kRepeat),
      {.kind = PlanKind::kUnvoiced, .k = kK4, .pitch = 0},
      {.kind = PlanKind::kRepeat, .k = kK4, .pitch = 30},
      {}};
  const auto eff = EffectiveK(plan);
  ASSERT_EQ(eff.size(), 5U);
  EXPECT_EQ(eff[0].size(), 10U);
  EXPECT_EQ(eff[1], eff[0]);
  EXPECT_EQ(eff[2].size(), 4U);
  // A voiced repeat after an unvoiced full frame uses K1-K4 only.
  EXPECT_EQ(eff[3].size(), 4U);
  EXPECT_TRUE(eff[4].empty());
  EXPECT_EQ(eff[0][0], lpc::kK1Table[20] / 512.0);
}

TEST(LevelSetterTest, OutputLevelsAreMeasuredAtEachFramesReachedSample) {
  std::vector<int> codes(2000, 10);
  codes[275] = 50;  // inside frame 0's window (225 +- 100) only
  const OutputLevels out = MeasureOutputLevels(codes, 3);
  ASSERT_EQ(out.level.size(), 3U);
  EXPECT_NEAR(out.level[2], 10.0, kTolerance);
  EXPECT_EQ(out.peak[0], 50);
  EXPECT_EQ(out.peak[1], 10);
  EXPECT_EQ(out.peak[2], 10);
}

TEST(LevelSetterTest, OutputLevelsPastTheEndAreZero) {
  const OutputLevels out = MeasureOutputLevels(std::vector<int>(100, 5), 2);
  EXPECT_EQ(out.level[1], 0.0);
  EXPECT_EQ(out.peak[1], 0);
}

TEST(LevelSetterTest, ShapingIsOffByDefault) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  const std::vector<double> levels = {0.1, 0.01, 0.05};
  const std::vector<PlanEntry> plan = {Voiced(), Voiced(), {}};
  EXPECT_EQ(setter.ShapeLevels(levels, plan, {}), levels);
}

TEST(LevelSetterTest, CompressionDividesDecibelsAroundTheMedian) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  EncoderOptions options;
  options.compression = 2.0;
  const std::vector<double> levels = {1.0, 0.01, 0.5};
  const std::vector<PlanEntry> plan = {Voiced(), Voiced(), {}};
  const std::vector<double> out = setter.ShapeLevels(levels, plan, options);
  // Sounding levels 0 and -40 dB, median -20: they become -10 and -30.
  EXPECT_NEAR(out[0], std::pow(10.0, -0.5), kTolerance);
  EXPECT_NEAR(out[1], std::pow(10.0, -1.5), kTolerance);
  EXPECT_NEAR(out[2], 0.5, kTolerance);  // silence is left alone
}

TEST(LevelSetterTest, SlowGainControlLeavesASteadyLevelAlone) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  EncoderOptions options;
  options.level_agc = 4.0;
  const std::vector<double> levels(40, 0.2);
  const std::vector<PlanEntry> plan(40, Voiced());
  for (const double v : setter.ShapeLevels(levels, plan, options)) {
    EXPECT_NEAR(v, 0.2, 1e-9);
  }
}

TEST(LevelSetterTest, SlowGainControlBringsAQuietStretchUp) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  EncoderOptions options;
  options.level_agc = 4.0;
  std::vector<double> levels(80, 0.2);
  std::fill(levels.begin() + 40, levels.end(), 0.02);
  const std::vector<PlanEntry> plan(80, Voiced());
  const std::vector<double> out = setter.ShapeLevels(levels, plan, options);
  EXPECT_GT(out[70], 0.02);
  EXPECT_LT(out[10], 0.2);
}

TEST(LevelSetterTest, SetLevelsKeepsSilenceAndRespectsThePeakLimit) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  EncoderOptions options;
  options.peak_limit = 40.0;
  std::vector<PlanEntry> plan(6, Voiced(PlanKind::kRepeat));
  plan[0] = Voiced();
  plan[3] = {};
  plan[4] = Voiced();
  const std::vector<double> levels(6, 0.1);
  LevelStats stats;
  const std::vector<int> energies =
      setter.SetLevels(plan, levels, 500.0, options, &stats);
  ASSERT_EQ(energies.size(), 6U);
  EXPECT_EQ(energies[3], 0);
  EXPECT_EQ(stats.sounding, 5);
  for (std::size_t j = 0; j < energies.size(); ++j) {
    EXPECT_GE(energies[j], j == 3 ? 0 : 1);
    EXPECT_LE(energies[j], 14);
  }
  const synth::ReferenceSynthesis out = synth::ReferenceSynthesizer::Synthesize(
      AddFades(ToFrames(plan, energies), options.fade_unvoiced));
  int peak = 0;
  for (const int c : out.codes) {
    peak = std::max(peak, std::abs(c));
  }
  EXPECT_LE(peak, 40);
  EXPECT_EQ(out.wrapped_samples, 0);
}

TEST(LevelSetterTest, LouderTargetGivesHigherEnergies) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  const std::vector<PlanEntry> plan = {Voiced(), Voiced(PlanKind::kRepeat),
                                       Voiced(PlanKind::kRepeat)};
  const std::vector<double> levels(3, 0.1);
  const std::vector<int> quiet =
      setter.SetLevels(plan, levels, 20.0, {}, nullptr);
  const std::vector<int> loud =
      setter.SetLevels(plan, levels, 200.0, {}, nullptr);
  EXPECT_LT(quiet[1], loud[1]);
}

}  // namespace
}  // namespace speak5220::core::encoder
