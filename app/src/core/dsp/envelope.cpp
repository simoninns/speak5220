/*
 * File:        envelope.cpp
 * Module:      dsp
 * Purpose:     The level and gain envelopes the preprocessing's dynamics
 *              (noise gate, de-esser) share: a centred RMS level, and
 *              gains whose changes are spread into ramps
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/envelope.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {
namespace {

constexpr double kMillisecondsPerSecond = 1000.0;
constexpr double kDecibelsPerDecade = 20.0;

}  // namespace

std::size_t SamplesOf(double ms, int sample_rate) {
  const auto samples = static_cast<std::size_t>(
      std::lround(ms * sample_rate / kMillisecondsPerSecond));
  return std::max<std::size_t>(samples, 1);
}

std::vector<double> CentredRms(std::span<const double> samples, int sample_rate,
                               double window_ms) {
  const std::size_t n = samples.size();
  const std::size_t half_window = SamplesOf(window_ms, sample_rate) / 2;
  std::vector<double> energy(n + 1, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    energy[i + 1] = energy[i] + (samples[i] * samples[i]);
  }
  std::vector<double> level(n);
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t begin = i > half_window ? i - half_window : 0;
    const std::size_t end = std::min(i + half_window + 1, n);
    level[i] = std::sqrt(std::max(energy[end] - energy[begin], 0.0) /
                         static_cast<double>(end - begin));
  }
  return level;
}

void SpreadDips(std::vector<double>& gain_db, double after_step,
                double before_step) {
  const std::size_t n = gain_db.size();
  for (std::size_t i = 1; i < n; ++i) {
    gain_db[i] = std::min(gain_db[i], gain_db[i - 1] + after_step);
  }
  for (std::size_t i = n; i-- > 1;) {
    gain_db[i - 1] = std::min(gain_db[i - 1], gain_db[i] + before_step);
  }
}

void SpreadPeaks(std::vector<double>& gain_db, double after_step,
                 double before_step) {
  const std::size_t n = gain_db.size();
  for (std::size_t i = 1; i < n; ++i) {
    gain_db[i] = std::max(gain_db[i], gain_db[i - 1] - after_step);
  }
  for (std::size_t i = n; i-- > 1;) {
    gain_db[i - 1] = std::max(gain_db[i - 1], gain_db[i] - before_step);
  }
}

std::vector<double> DbToGain(std::span<const double> gain_db) {
  std::vector<double> gain(gain_db.size());
  for (std::size_t i = 0; i < gain.size(); ++i) {
    gain[i] = std::pow(10.0, gain_db[i] / kDecibelsPerDecade);
  }
  return gain;
}

}  // namespace speak5220::core::dsp
