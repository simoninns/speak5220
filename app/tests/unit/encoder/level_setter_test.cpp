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
#include <numbers>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder_filters.h"
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

TEST(LevelSetterTest, ShapingIsOffWithoutGainControl) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  EncoderOptions options;
  options.level_agc = 1.0;
  const std::vector<double> levels = {0.1, 0.01, 0.05};
  const std::vector<PlanEntry> plan = {Voiced(), Voiced(), {}};
  EXPECT_EQ(setter.ShapeLevels(levels, plan, options), levels);
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

// The samples of `out` beyond the DAC's +-127.
int Clipped(const synth::ReferenceSynthesis& out) {
  return static_cast<int>(std::ranges::count_if(
      out.codes, [](int c) { return std::abs(c) > 127; }));
}

int Peak(const synth::ReferenceSynthesis& out) {
  int peak = 0;
  for (const int c : out.codes) {
    peak = std::max(peak, std::abs(c));
  }
  return peak;
}

// Equal-amplitude tones of 100 Hz and 1 kHz: the same level, but the low
// tone's loudness is that of the loudness high-pass at 100 Hz.
TEST(LevelSetterTest, OutputLoudnessIsTheLevelAfterTheLoudnessHighPass) {
  std::vector<int> low(4000);
  std::vector<int> high(4000);
  for (std::size_t n = 0; n < low.size(); ++n) {
    const double t = 2.0 * std::numbers::pi * static_cast<double>(n) / 8000.0;
    low[n] = static_cast<int>(std::lround(100.0 * std::sin(100.0 * t)));
    high[n] = static_cast<int>(std::lround(100.0 * std::sin(1000.0 * t)));
  }
  const OutputLevels low_level = MeasureOutputLevels(low, 10);
  const OutputLevels low_loud = MeasureOutputLoudness(low, 10);
  const OutputLevels high_loud = MeasureOutputLoudness(high, 10);
  // |H|^2 at bins of 50 Hz: 100 Hz is bin 2, 1 kHz bin 20.
  const std::vector<double> power = LoudnessHighPassPower(81);
  EXPECT_NEAR(low_loud.level[8] / low_level.level[8], std::sqrt(power[2]),
              0.01);
  EXPECT_NEAR(high_loud.level[8] / MeasureOutputLevels(high, 10).level[8],
              std::sqrt(power[20]), 0.01);
  // The peaks are the codes' own.
  EXPECT_EQ(low_loud.peak, low_level.peak);
}

TEST(LevelSetterTest, LoudnessGainOfAFlatFilterWeighsTheNoiseByTheHighPass) {
  dsp::PocketFft fft;
  // A flat filter's noise output is white, so its loudness is its RMS (0.5,
  // above) times the root of the high-pass's mean power gain.
  const std::vector<double> power = LoudnessHighPassPower(2049);
  std::vector<double> full(power.begin(), power.end());
  full.insert(full.end(), power.begin() + 1, power.end() - 1);
  double mean = 0.0;
  for (const double v : full) {
    mean += v;
  }
  mean /= static_cast<double>(full.size());
  EXPECT_NEAR(GainOf(fft, {}, 0, /*loudness=*/true), 0.5 * std::sqrt(mean),
              kTolerance);
  EXPECT_LT(GainOf(fft, {}, 100, /*loudness=*/true), GainOf(fft, {}, 100));
}

// No lattice wrap, no code beyond 160, and at most 1 sample in 1000 beyond
// the DAC's +-127, which the limiter allows rather than holding every code
// within 120 as the reference does.
TEST(LevelSetterTest, ClipsNoMoreThanTheBudgetAndPassesNoPeakOverTheCeiling) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  std::vector<PlanEntry> plan(60, Voiced(PlanKind::kRepeat));
  for (std::size_t j = 0; j < plan.size(); j += 10) {
    plan[j] = Voiced();
  }
  const std::vector<double> levels(plan.size(), 0.1);
  bool clipped_any = false;
  for (const double gain : {100.0, 200.0, 300.0, 500.0, 2000.0}) {
    const std::vector<int> energies =
        setter.SetLevels(plan, levels, gain, nullptr);
    const synth::ReferenceSynthesis out =
        synth::ReferenceSynthesizer::Synthesize(
            AddFades(ToFrames(plan, energies)));
    EXPECT_EQ(out.wrapped_samples, 0) << gain;
    EXPECT_LE(Peak(out), 160) << gain;
    EXPECT_LE(Clipped(out), static_cast<int>(out.codes.size() / 1000)) << gain;
    clipped_any = clipped_any || Clipped(out) > 0;
  }
  EXPECT_TRUE(clipped_any);
}

TEST(LevelSetterTest, SetLevelsKeepsSilenceAndRespectsThePeakLimit) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  std::vector<PlanEntry> plan(6, Voiced(PlanKind::kRepeat));
  plan[0] = Voiced();
  plan[3] = {};
  plan[4] = Voiced();
  const std::vector<double> levels(6, 0.1);
  LevelStats stats;
  const std::vector<int> energies =
      setter.SetLevels(plan, levels, 2000.0, &stats);
  ASSERT_EQ(energies.size(), 6U);
  EXPECT_EQ(energies[3], 0);
  EXPECT_EQ(stats.sounding, 5);
  for (std::size_t j = 0; j < energies.size(); ++j) {
    EXPECT_GE(energies[j], j == 3 ? 0 : 1);
    EXPECT_LE(energies[j], 14);
  }
  const synth::ReferenceSynthesis out = synth::ReferenceSynthesizer::Synthesize(
      AddFades(ToFrames(plan, energies)));
  EXPECT_LE(Peak(out), 160);
  EXPECT_LE(Clipped(out), static_cast<int>(out.codes.size() / 1000));
  EXPECT_EQ(out.wrapped_samples, 0);
}

TEST(LevelSetterTest, LouderTargetGivesHigherEnergies) {
  dsp::PocketFft fft;
  LevelSetter setter(fft);
  const std::vector<PlanEntry> plan = {Voiced(), Voiced(PlanKind::kRepeat),
                                       Voiced(PlanKind::kRepeat)};
  const std::vector<double> levels(3, 0.1);
  const std::vector<int> quiet = setter.SetLevels(plan, levels, 20.0, nullptr);
  const std::vector<int> loud = setter.SetLevels(plan, levels, 200.0, nullptr);
  EXPECT_LT(quiet[1], loud[1]);
}

}  // namespace
}  // namespace speak5220::core::encoder
