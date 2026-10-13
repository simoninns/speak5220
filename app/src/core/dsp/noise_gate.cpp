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

namespace speak5220::core::dsp {
namespace {

constexpr double kMillisecondsPerSecond = 1000.0;
constexpr double kDecibelsPerDecade = 20.0;

// A duration in samples, at least 1.
std::size_t SamplesOf(double ms, int sample_rate) {
  const auto samples = static_cast<std::size_t>(
      std::lround(ms * sample_rate / kMillisecondsPerSecond));
  return std::max<std::size_t>(samples, 1);
}

}  // namespace

std::vector<double> NoiseGateGain(std::span<const double> samples,
                                  int sample_rate, double below_db,
                                  double close_ms,
                                  const NoiseGateShape& shape) {
  const std::size_t n = samples.size();
  if (n == 0) {
    return {};
  }
  const std::size_t half_window = SamplesOf(shape.window_ms, sample_rate) / 2;

  // Each sample's level: the RMS of the window centred on it, from a
  // running sum of squares so each is O(1).
  std::vector<double> energy(n + 1, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    energy[i + 1] = energy[i] + (samples[i] * samples[i]);
  }
  std::vector<double> level(n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t begin = i > half_window ? i - half_window : 0;
    const std::size_t end = std::min(i + half_window + 1, n);
    level[i] = std::sqrt((energy[end] - energy[begin]) /
                         static_cast<double>(end - begin));
  }
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

  // Closing no faster than the speed allows, after each sound...
  const double range_db = -shape.floor_db;
  const double close_step =
      range_db / static_cast<double>(SamplesOf(close_ms, sample_rate));
  for (std::size_t i = 1; i < n; ++i) {
    gain_db[i] = std::max(gain_db[i], gain_db[i - 1] - close_step);
  }
  // ...and opening no faster than its share of it, before each, so the
  // gate is open when the sound starts.
  const double open_step =
      range_db /
      static_cast<double>(SamplesOf(close_ms * shape.open_share, sample_rate));
  for (std::size_t i = n - 1; i-- > 0;) {
    gain_db[i] = std::max(gain_db[i], gain_db[i + 1] - open_step);
  }

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
