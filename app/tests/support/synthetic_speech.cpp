/*
 * File:        synthetic_speech.cpp
 * Module:      test support
 * Purpose:     A deterministic speech-like test signal (voiced bursts, a
 *              fricative and pauses) for unit tests of the quality measures
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/synthetic_speech.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <utility>
#include <vector>

#include "core/quality/numpy_normal.h"

namespace speak5220::testing {
namespace {

constexpr double kRate = 8000.0;

// The gain of three formant resonances (500, 1500 and 2500 Hz, 100 Hz
// wide) at `hz`.
double FormantGain(double hz) {
  constexpr std::array<double, 3> kFormants = {500.0, 1500.0, 2500.0};
  constexpr double kWidth = 100.0;
  double g = 0.0;
  for (double f : kFormants) {
    const double d = (hz - f) / kWidth;
    g += 1.0 / (1.0 + d * d);
  }
  return g;
}

// A vowel of `n` samples whose fundamental glides from f_start to f_end,
// with a raised-cosine envelope.
void AddVowel(std::vector<double>& x, int start, int n, double f_start,
              double f_end, double peak) {
  double phase = 0.0;
  for (int i = 0; i < n; ++i) {
    const double t = static_cast<double>(i) / n;
    const double f0 = f_start + (f_end - f_start) * t;
    phase += 2 * std::numbers::pi * f0 / kRate;
    double v = 0.0;
    for (int h = 1; f0 * h < 3400.0; ++h) {
      v += FormantGain(f0 * h) * std::sin(h * phase) / h;
    }
    const double envelope = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * t);
    x[static_cast<std::size_t>(start) + static_cast<std::size_t>(i)] +=
        peak * 0.25 * envelope * v;
  }
}

}  // namespace

std::vector<double> SyntheticSpeech() {
  constexpr int kLength = 16000;
  std::vector<double> x(kLength, 0.0);
  AddVowel(x, 1600, 4800, 110.0, 140.0, 0.5);
  // The fricative: differenced noise with a raised-cosine envelope.
  constexpr int kFricativeStart = 7600;
  constexpr int kFricativeLength = 2000;
  const std::vector<double> noise = WhiteNoise(kFricativeLength + 1, 0.15);
  for (int i = 0; i < kFricativeLength; ++i) {
    const double t = static_cast<double>(i) / kFricativeLength;
    const double envelope = 0.5 - 0.5 * std::cos(2 * std::numbers::pi * t);
    x[static_cast<std::size_t>(kFricativeStart) +
      static_cast<std::size_t>(i)] +=
        envelope * (noise[static_cast<std::size_t>(i) + 1] -
                    noise[static_cast<std::size_t>(i)]);
  }
  AddVowel(x, kSecondVowelStart, kSecondVowelEnd - kSecondVowelStart,
           kSecondVowelHz, kSecondVowelHz, 0.4);
  return x;
}

std::vector<double> Delayed(const std::vector<double>& x, int delay) {
  std::vector<double> y(x.size(), 0.0);
  for (std::size_t i = 0; i < x.size(); ++i) {
    const std::ptrdiff_t j = static_cast<std::ptrdiff_t>(i) - delay;
    if (j >= 0 && std::cmp_less(j, x.size())) {
      y[i] = x[static_cast<std::size_t>(j)];
    }
  }
  return y;
}

std::vector<double> WhiteNoise(int n, double rms) {
  core::quality::NumpyNormal normal = core::quality::NumpyNormal::SeedOne();
  std::vector<double> y(static_cast<std::size_t>(n));
  for (double& v : y) {
    v = rms * normal.Next();
  }
  return y;
}

}  // namespace speak5220::testing
