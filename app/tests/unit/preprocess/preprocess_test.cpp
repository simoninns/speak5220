/*
 * File:        preprocess_test.cpp
 * Module:      preprocess tests
 * Purpose:     Unit tests of the preprocessing: the noise gate, the bass
 *              cut, the treble tilt and the de-esser, the Butterworth
 *              high-pass behind the cut, and the presets
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/preprocess/preprocess.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "core/dsp/iir_filter.h"
#include "core/dsp/noise_gate.h"
#include "gtest/gtest.h"

namespace speak5220::core::preprocess {
namespace {

// scipy.signal.butter(2, 200, "highpass", fs=8000, output="sos") and
// sosfilt_zi() of it.
TEST(ButterworthHighPassTest, MatchesScipysDesign) {
  const dsp::SosSection s = ButterworthHighPass(200.0);
  EXPECT_NEAR(s.b[0], 0.8948586061225731, 1e-12);
  EXPECT_NEAR(s.b[1], -1.7897172122451461, 1e-12);
  EXPECT_NEAR(s.b[2], 0.8948586061225731, 1e-12);
  EXPECT_NEAR(s.a[0], -1.7786317778245846, 1e-12);
  EXPECT_NEAR(s.a[1], 0.8008026466657073, 1e-12);
  EXPECT_NEAR(s.zi[0], -0.8948586061225737, 1e-12);
  EXPECT_NEAR(s.zi[1], 0.8948586061225736, 1e-12);
}

// The RMS of a 1 s sine at `hz` (8 kHz) preprocessed by `options`, over
// its middle half (away from the ends).
double PreprocessedRms(double hz, const PreprocessOptions& options) {
  std::vector<double> x(8000);
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] =
        std::sin(2.0 * std::numbers::pi * hz * static_cast<double>(i) / 8000.0);
  }
  const std::vector<double> y = Preprocess(x, options).samples;
  double sum = 0.0;
  for (std::size_t i = 2000; i < 6000; ++i) {
    sum += y[i] * y[i];
  }
  return std::sqrt(sum / 4000.0);
}

// Loud, then 60 dB quieter, half a second each.
std::vector<double> LoudThenQuiet() {
  std::vector<double> samples(8000, 0.5);
  std::fill(samples.begin() + 4000, samples.end(), 0.0005);
  return samples;
}

TEST(PreprocessTest, WithEverythingOffBothAreTheOriginal) {
  const std::vector<double> x = {0.5, -0.25, 0.125};
  const PreprocessedAudio audio = Preprocess(x, {});
  EXPECT_EQ(audio.samples, x);
  EXPECT_EQ(audio.voicing, x);
}

TEST(PreprocessTest, NoiseGateMutesTheQuietStretchOfBoth) {
  const std::vector<double> x = LoudThenQuiet();
  const PreprocessedAudio audio = Preprocess(x, {.noise_gate_db = 30.0});
  EXPECT_EQ(audio.samples, dsp::NoiseGate(x, kPreprocessSampleRate, 30.0,
                                          kDefaultNoiseGateMs));
  EXPECT_EQ(audio.voicing, audio.samples);
  EXPECT_EQ(audio.samples.back(), 0.0);
  // The gate closes at the speed asked for.
  EXPECT_EQ(
      Preprocess(x, {.noise_gate_db = 30.0, .noise_gate_ms = 250.0}).samples,
      dsp::NoiseGate(x, kPreprocessSampleRate, 30.0, 250.0));
}

TEST(PreprocessTest, BassCutIsSixDecibelsDownAtTheCutAndZeroPhase) {
  const PreprocessOptions options{.bass_cut_hz = 200.0};
  const double pass = PreprocessedRms(2000.0, options);
  EXPECT_NEAR(pass, std::sqrt(0.5), 0.01);
  // Forwards and backwards: twice the 3 dB of one pass.
  EXPECT_NEAR(20.0 * std::log10(PreprocessedRms(200.0, options) / pass), -6.0,
              0.1);
  // 24 dB per octave below the cut.
  EXPECT_LT(20.0 * std::log10(PreprocessedRms(50.0, options) / pass), -45.0);
}

TEST(PreprocessTest, TrebleTiltLiftsTheTreble) {
  const PreprocessOptions options{.treble_tilt = 0.4};
  // |1 - 0.4 e^-jw|: 0.6 at DC, 1.4 at the Nyquist frequency.
  const double low = PreprocessedRms(100.0, options);
  const double high = PreprocessedRms(3000.0, options);
  EXPECT_GT(high / low, 2.0);
}

// The filters come after the noise gate, and the voicing recording is the
// gated one before them.
TEST(PreprocessTest, FiltersFollowTheNoiseGateAndLeaveTheVoicingRecording) {
  const std::vector<double> x = LoudThenQuiet();
  const PreprocessOptions options{
      .noise_gate_db = 30.0, .bass_cut_hz = 150.0, .treble_tilt = 0.3};
  const PreprocessedAudio audio = Preprocess(x, options);
  const std::vector<double> gated =
      dsp::NoiseGate(x, kPreprocessSampleRate, 30.0, kDefaultNoiseGateMs);
  EXPECT_EQ(audio.voicing, gated);
  EXPECT_EQ(
      audio.samples,
      Preprocess(gated, {.bass_cut_hz = 150.0, .treble_tilt = 0.3}).samples);
  EXPECT_NE(audio.samples, audio.voicing);
}

// The de-esser turns down the treble band where it is most of the sound,
// and leaves the voicing recording alone.
TEST(PreprocessTest, DeEsserTurnsDownTreblyStretchesOnly) {
  const PreprocessOptions options{.de_ess_db = 12.0};
  EXPECT_NEAR(
      20.0 * std::log10(PreprocessedRms(3500.0, options) / std::sqrt(0.5)),
      -12.0, 0.5);
  EXPECT_NEAR(PreprocessedRms(300.0, options), std::sqrt(0.5), 1e-3);
  const std::vector<double> x = LoudThenQuiet();
  EXPECT_EQ(Preprocess(x, options).voicing, x);
}

TEST(GatedSpansTest, AreTheGatesSpansOrNoneWithoutIt) {
  const std::vector<double> x = LoudThenQuiet();
  EXPECT_TRUE(GatedSpans(x, {}).empty());
  EXPECT_EQ(
      GatedSpans(x, {.noise_gate_db = 30.0}),
      dsp::GatedSpans(x, kPreprocessSampleRate, 30.0, kDefaultNoiseGateMs));
}

TEST(PreprocessPresetsTest, AreNoneAndBbcMicro) {
  const auto presets = PreprocessPresets();
  ASSERT_EQ(presets.size(), 2U);
  EXPECT_EQ(presets[0].name, "None");
  EXPECT_EQ(presets[0].options, PreprocessOptions{});
  EXPECT_EQ(presets[1].name, "BBC Micro");
  EXPECT_EQ(presets[1].options,
            (PreprocessOptions{.bass_cut_hz = 200.0, .treble_tilt = 0.4}));
}

TEST(PreprocessPresetsTest, MatchingPresetFindsTheOneWithTheseOptions) {
  EXPECT_EQ(MatchingPreset({})->name, "None");
  EXPECT_EQ(MatchingPreset({.bass_cut_hz = 200.0, .treble_tilt = 0.4})->name,
            "BBC Micro");
  EXPECT_EQ(MatchingPreset({.bass_cut_hz = 210.0, .treble_tilt = 0.4}),
            nullptr);
  EXPECT_EQ(MatchingPreset({.noise_gate_db = 30.0}), nullptr);
  EXPECT_EQ(MatchingPreset({.de_ess_db = 10.0}), nullptr);
}

// The gate's speed does nothing while the gate is off, so it does not stop
// a preset matching.
TEST(PreprocessPresetsTest, GatesSpeedMattersOnlyWithTheGate) {
  EXPECT_EQ(MatchingPreset({.noise_gate_ms = 300.0})->name, "None");
  EXPECT_EQ(
      MatchingPreset(
          {.noise_gate_ms = 300.0, .bass_cut_hz = 200.0, .treble_tilt = 0.4})
          ->name,
      "BBC Micro");
}

}  // namespace
}  // namespace speak5220::core::preprocess
