/*
 * File:        encoder_filters.cc
 * Module:      encoder
 * Purpose:     The encoder's fixed filters: the 60 Hz high-pass, the 1 kHz
 *              low-pass and the analysis pre-emphasis
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/encoder_filters.h"

#include <cstddef>
#include <span>
#include <vector>

#include "core/dsp/filter_constants.h"
#include "core/dsp/iir_filter.h"
#include "core/dsp/lpc_analysis.h"
#include "core/lpc/coding_tables.h"

namespace speak5220::core::encoder {
namespace {

// tms5220enc.py _sosfiltfilt() leaves inputs this short unfiltered.
constexpr std::size_t kShortestFiltered = 64;
// The chirp's non-zero samples, and the order of its inverse.
constexpr std::size_t kChirpLength = 21;
constexpr int kChirpInverseOrder = 4;

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

std::vector<double> PreEmphasisFilter(const PreEmphasis& pre_emphasis) {
  if (pre_emphasis.chirp) {
    return ChirpInverse();
  }
  return {1.0, -pre_emphasis.mu};
}

}  // namespace speak5220::core::encoder
