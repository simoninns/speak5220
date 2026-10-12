/*
 * File:        squelch.cpp
 * Module:      dsp
 * Purpose:     Mutes the quiet stretches of a recording (breaths, room
 *              noise) relative to its loudest sound, with a hold and soft
 *              edges so word endings survive
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/squelch.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {
namespace {

constexpr double kMillisecondsPerSecond = 1000.0;

// A duration in samples, at least `minimum`.
std::size_t SamplesOf(double ms, int sample_rate, std::size_t minimum) {
  const auto samples = static_cast<std::size_t>(
      std::lround(ms * sample_rate / kMillisecondsPerSecond));
  return std::max(samples, minimum);
}

}  // namespace

std::vector<double> SquelchGain(std::span<const double> samples,
                                int sample_rate, double below_db,
                                const SquelchTiming& timing) {
  const std::size_t n = samples.size();
  if (n == 0) {
    return {};
  }
  const std::size_t block = SamplesOf(timing.block_ms, sample_rate, 1);
  const std::size_t hop = SamplesOf(timing.hop_ms, sample_rate, 1);
  const std::size_t hold = SamplesOf(timing.hold_ms, sample_rate, 0);
  const std::size_t lead = SamplesOf(timing.lead_ms, sample_rate, 0);
  const std::size_t ramp = SamplesOf(timing.ramp_ms, sample_rate, 1);

  // The running sum of squares, so each block's RMS is O(1).
  std::vector<double> energy(n + 1, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    energy[i + 1] = energy[i] + (samples[i] * samples[i]);
  }
  // Each block's RMS, over the samples it covers.
  std::vector<double> levels;
  for (std::size_t start = 0; start < n; start += hop) {
    const std::size_t end = std::min(start + block, n);
    levels.push_back(std::sqrt((energy[end] - energy[start]) /
                               static_cast<double>(end - start)));
  }
  const double loudest = *std::ranges::max_element(levels);
  if (loudest <= 0.0) {
    std::vector<double> unity(n, 1.0);
    return unity;
  }
  const double threshold = loudest * std::pow(10.0, -below_db / 20.0);

  // The samples covered by an open block (a difference array of the
  // blocks' spans).
  std::vector<int> cover(n + 1, 0);
  for (std::size_t k = 0; k < levels.size(); ++k) {
    if (levels[k] > threshold) {
      const std::size_t start = k * hop;
      ++cover[start];
      --cover[std::min(start + block, n)];
    }
  }
  std::vector<double> gain(n, 0.0);
  int open_blocks = 0;
  for (std::size_t i = 0; i < n; ++i) {
    open_blocks += cover[i];
    gain[i] = open_blocks > 0 ? 1.0 : 0.0;
  }

  // Widened by the hold after each open stretch and the lead before it.
  std::vector<double> held(gain);
  std::size_t since_open = hold + 1;
  for (std::size_t i = 0; i < n; ++i) {
    since_open = gain[i] > 0.0 ? 0 : since_open + 1;
    if (since_open <= hold) {
      held[i] = 1.0;
    }
  }
  std::size_t until_open = lead + 1;
  for (std::size_t i = n; i-- > 0;) {
    until_open = gain[i] > 0.0 ? 0 : until_open + 1;
    if (until_open <= lead) {
      held[i] = 1.0;
    }
  }

  // Ramps outside each open stretch: down after it, up before it.
  const double step = 1.0 / static_cast<double>(ramp);
  for (std::size_t i = 1; i < n; ++i) {
    held[i] = std::max(held[i], held[i - 1] - step);
  }
  for (std::size_t i = n - 1; i-- > 0;) {
    held[i] = std::max(held[i], held[i + 1] - step);
  }
  return held;
}

std::vector<double> Squelch(std::span<const double> samples, int sample_rate,
                            double below_db, const SquelchTiming& timing) {
  std::vector<double> out = SquelchGain(samples, sample_rate, below_db, timing);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] *= samples[i];
  }
  return out;
}

std::vector<SampleSpan> SquelchedSpans(std::span<const double> samples,
                                       int sample_rate, double below_db,
                                       const SquelchTiming& timing) {
  const std::vector<double> gain =
      SquelchGain(samples, sample_rate, below_db, timing);
  std::vector<SampleSpan> spans;
  for (std::size_t i = 0; i < gain.size(); ++i) {
    if (gain[i] >= 1.0) {
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
