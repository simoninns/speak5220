/*
 * File:        noise_gate.cpp
 * Module:      dsp
 * Purpose:     Turns down the quiet stretches of a recording (breaths, room
 *              noise) relative to its loudest sound, with a soft knee and
 *              gradual opening and closing so word edges survive
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/noise_gate.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/dsp/envelope.h"

namespace speak5220::core::dsp {
namespace {

constexpr double kDecibelsPerDecade = 20.0;

}  // namespace

std::vector<double> NoiseGateGain(std::span<const double> samples,
                                  int sample_rate, double below_db,
                                  double close_ms,
                                  const NoiseGateShape& shape) {
  const std::size_t n = samples.size();
  if (n == 0) {
    return {};
  }
  // Each sample's level: the RMS of the window centred on it.
  const std::vector<double> level =
      CentredRms(samples, sample_rate, shape.window_ms);
  const double loudest = *std::ranges::max_element(level);
  if (loudest <= 0.0) {
    std::vector<double> unity(n, 1.0);
    return unity;
  }
  const double threshold =
      loudest * std::pow(10.0, -below_db / kDecibelsPerDecade);

  // The gain each level asks for, in dB: open at or above the threshold,
  // falling by the expansion below it, down to the floor.
  std::vector<double> gain_db(n);
  for (std::size_t i = 0; i < n; ++i) {
    if (level[i] >= threshold) {
      gain_db[i] = 0.0;
    } else if (level[i] <= 0.0) {
      gain_db[i] = shape.floor_db;
    } else {
      const double under_db =
          kDecibelsPerDecade * std::log10(level[i] / threshold);
      gain_db[i] =
          std::max(shape.floor_db, shape.expansion_db_per_db * under_db);
    }
  }

  // Closing no faster than the speed allows, after each sound, and
  // opening no faster than its share of it, before each, so the gate is
  // open when the sound starts.
  const double range_db = -shape.floor_db;
  SpreadPeaks(gain_db,
              range_db / static_cast<double>(SamplesOf(close_ms, sample_rate)),
              range_db / static_cast<double>(SamplesOf(
                             close_ms * shape.open_share, sample_rate)));

  std::vector<double> gain(n);
  for (std::size_t i = 0; i < n; ++i) {
    gain[i] = gain_db[i] <= shape.floor_db
                  ? 0.0
                  : std::pow(10.0, gain_db[i] / kDecibelsPerDecade);
  }
  return gain;
}

std::vector<double> NoiseGate(std::span<const double> samples, int sample_rate,
                              double below_db, double close_ms,
                              const NoiseGateShape& shape) {
  std::vector<double> out =
      NoiseGateGain(samples, sample_rate, below_db, close_ms, shape);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] *= samples[i];
  }
  return out;
}

std::vector<SampleSpan> GatedSpans(std::span<const double> samples,
                                   int sample_rate, double below_db,
                                   double close_ms,
                                   const NoiseGateShape& shape) {
  const std::vector<double> gain =
      NoiseGateGain(samples, sample_rate, below_db, close_ms, shape);
  std::vector<SampleSpan> spans;
  for (std::size_t i = 0; i < gain.size(); ++i) {
    if (gain[i] >= kGatedGain) {
      continue;
    }
    if (!spans.empty() && spans.back().end == i) {
      spans.back().end = i + 1;
    } else {
      spans.push_back({.begin = i, .end = i + 1});
    }
  }
  return spans;
}

}  // namespace speak5220::core::dsp
