/*
 * File:        encoder_filters.cpp
 * Module:      encoder
 * Purpose:     The encoder's fixed filters: the 60 Hz high-pass, the 1 kHz
 *              low-pass, the analysis pre-emphasis and the loudness
 *              high-pass
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/encoder_filters.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

#include "core/dsp/filter_constants.h"
#include "core/dsp/iir_filter.h"
#include "core/dsp/lpc_analysis.h"
#include "core/encoder/encoder_types.h"
#include "core/lpc/coding_tables.h"

namespace speak5220::core::encoder {
namespace {

// tms5220enc.py _sosfiltfilt() leaves inputs this short unfiltered.
constexpr std::size_t kShortestFiltered = 64;
// The chirp's non-zero samples, and the order of its inverse.
constexpr std::size_t kChirpLength = 21;
constexpr int kChirpInverseOrder = 4;

// The prewarped bilinear transform's tan(pi fc / fs) for the loudness
// high-pass.
double LoudnessK() {
  return std::tan(std::numbers::pi * kLoudnessHighPassHz /
                  static_cast<double>(kEncoderSampleRate));
}

}  // namespace

std::vector<double> HighPass60Hz(std::span<const double> x) {
  if (x.size() < kShortestFiltered) {
    return {x.begin(), x.end()};
  }
  return dsp::SosFiltFilt(dsp::kHighPass60Hz, x);
}

std::vector<double> LowPass1kHz(std::span<const double> x) {
  if (x.size() < kShortestFiltered) {
    return {x.begin(), x.end()};
  }
  return dsp::SosFiltFilt(dsp::kLowPass1kHz, x);
}

std::vector<double> ChirpInverse() {
  std::vector<double> chirp;
  chirp.reserve(kChirpLength);
  for (std::size_t i = 0; i < kChirpLength; ++i) {
    chirp.push_back(static_cast<double>(lpc::kChirpTable[i]));
  }
  const std::vector<double> r = dsp::Autocorrelation(chirp, kChirpInverseOrder);
  return dsp::StepUp(dsp::Levinson(r, kChirpInverseOrder));
}

std::vector<double> LoudnessHighPass(std::span<const double> x) {
  // H(z) = (1 - z^-1) / ((1 + k) + (k - 1) z^-1).
  const double k = LoudnessK();
  const std::array<dsp::SosSection, 1> sections = {
      {{.b = {1.0 / (1.0 + k), -1.0 / (1.0 + k), 0.0},
        .a = {(k - 1.0) / (1.0 + k), 0.0},
        .zi = {}}}};
  std::vector<double> y(x.begin(), x.end());
  std::array<double, 2> state = {};
  dsp::SosFilterInPlace(sections, state, y);
  return y;
}

std::vector<double> LoudnessHighPassPower(std::size_t bins) {
  const double k = LoudnessK();
  std::vector<double> power(bins, 0.0);
  if (bins < 2) {
    return power;
  }
  for (std::size_t i = 0; i < bins; ++i) {
    // |1 - e^-jw|^2 / |(1 + k) + (k - 1) e^-jw|^2.
    const double w = std::numbers::pi * static_cast<double>(i) /
                     static_cast<double>(bins - 1);
    const double c = std::cos(w);
    const double num = 2.0 - (2.0 * c);
    const double den = ((1.0 + k) * (1.0 + k)) + ((k - 1.0) * (k - 1.0)) +
                       (2.0 * (1.0 + k) * (k - 1.0) * c);
    power[i] = num / den;
  }
  return power;
}

}  // namespace speak5220::core::encoder
