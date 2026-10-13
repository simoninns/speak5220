/*
 * File:        de_esser_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of the de-esser: how far it turns the treble band
 *              down for its share of the sound, how gradually, and what it
 *              leaves alone
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/de_esser.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

constexpr int kRate = 8000;
constexpr std::size_t kLength = 4000;
constexpr std::size_t kMiddle = kLength / 2;

std::vector<double> Sine(double hz, double amplitude) {
  std::vector<double> x(kLength);
  for (std::size_t i = 0; i < x.size(); ++i) {
    x[i] = amplitude * std::sin(2.0 * std::numbers::pi * hz *
                                static_cast<double>(i) / kRate);
  }
  return x;
}

std::vector<double> Sum(const std::vector<double>& a,
                        const std::vector<double>& b) {
  std::vector<double> out(a.size());
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = a[i] + b[i];
  }
  return out;
}

// Where the band is all of the sound, as in an S, it is turned down by the
// whole depth.
TEST(DeEsserTest, TurnsAnAllTrebleSoundDownByTheDepth) {
  const std::vector<double> hiss = Sine(3200.0, 0.5);
  const std::vector<double> gain = DeEsserGain(hiss, hiss, kRate, 12.0);
  EXPECT_NEAR(gain[kMiddle], std::pow(10.0, -12.0 / 20.0), 1e-9);
  const std::vector<double> y = DeEss(hiss, hiss, kRate, 12.0);
  EXPECT_NEAR(y[kMiddle], hiss[kMiddle] * gain[kMiddle], 1e-12);
}

// A vowel, whose treble is a small share of it, is left alone.
TEST(DeEsserTest, LeavesASoundWithLittleTrebleAlone) {
  const std::vector<double> treble = Sine(3200.0, 0.01);
  const std::vector<double> vowel = Sum(Sine(300.0, 0.5), treble);
  EXPECT_EQ(DeEss(vowel, treble, kRate, 12.0), vowel);
}

// Between the two, the band is turned down by the depth less how far its
// share is below the whole.
TEST(DeEsserTest, TurnsDownByHowMuchOfTheSoundIsTreble) {
  // Equal levels: the band is 3 dB below the whole.
  const std::vector<double> treble = Sine(3200.0, 0.3);
  const std::vector<double> mixed = Sum(Sine(500.0, 0.3), treble);
  const std::vector<double> gain = DeEsserGain(mixed, treble, kRate, 12.0);
  EXPECT_NEAR(20.0 * std::log10(gain[kMiddle]),
              -(12.0 + (20.0 * std::log10(std::sqrt(0.5)))), 0.05);
  // Only the band is turned down: the rest is still there.
  const std::vector<double> y = DeEss(mixed, treble, kRate, 12.0);
  const std::vector<double> rest = Sine(500.0, 0.3);
  EXPECT_NEAR(y[kMiddle] - (gain[kMiddle] * treble[kMiddle]), rest[kMiddle],
              1e-12);
}

// The gain changes by no more than 1 dB a millisecond (8 samples), and
// starts falling before the hiss does.
TEST(DeEsserTest, ChangesGraduallyAndAhead) {
  std::vector<double> x = Sine(300.0, 0.5);
  std::vector<double> band(kLength, 0.0);
  const std::vector<double> hiss = Sine(3200.0, 0.5);
  for (std::size_t i = kMiddle; i < kLength; ++i) {
    x[i] = hiss[i];
    band[i] = hiss[i];
  }
  const std::vector<double> gain = DeEsserGain(x, band, kRate, 12.0);
  for (std::size_t i = 1; i < gain.size(); ++i) {
    const double change = std::abs(20.0 * std::log10(gain[i] / gain[i - 1]));
    ASSERT_LE(change, (1.0 / 8.0) + 1e-9) << i;
  }
  EXPECT_LT(gain[kMiddle - 1], 1.0);
  EXPECT_EQ(gain[kMiddle - 200], 1.0);
}

TEST(DeEsserTest, SilenceAndNothingComeBackUnchanged) {
  const std::vector<double> silence(1000, 0.0);
  EXPECT_EQ(DeEss(silence, silence, kRate, 12.0), silence);
  EXPECT_TRUE(
      DeEss(std::vector<double>{}, std::vector<double>{}, kRate, 12.0).empty());
}

}  // namespace
}  // namespace speak5220::core::dsp
