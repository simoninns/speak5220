/*
 * File:        voicing_classifier.cpp
 * Module:      encoder
 * Purpose:     Each frame's voicing and period from the pitch track, with
 *              octave errors corrected against its neighbours and short
 *              islands of voicing removed
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/voicing_classifier.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/pitch_tracker.h"

namespace speak5220::core::encoder {
namespace {

// The octave fix: a period this far above or below its neighbours' median
// moves an octave, if the result stays within the pitch range (§8.1, Table
// 8-5: 16 to 160 samples).
constexpr double kHighRatio = 1.7;
constexpr double kLowRatio = 0.6;
constexpr double kShortestPeriod = 16.0;
constexpr double kLongestPeriod = 160.0;
constexpr int kNeighbourReach = 2;
constexpr std::size_t kMinimumNeighbours = 2;

}  // namespace

std::vector<FrameVoicing> VoicingClassifier::Classify(
    const PitchTrack& track, std::span<const int> centres) {
  const auto hops = static_cast<int>(track.periods.size());
  std::vector<FrameVoicing> out;
  out.reserve(centres.size());
  std::vector<double> voiced_periods;
  for (const int c : centres) {
    // Python's round(): halves go to the even neighbour.
    const auto h0 = static_cast<int>(std::nearbyint(
        static_cast<double>(c) / static_cast<double>(kPitchHop)));
    int counted = 0;
    voiced_periods.clear();
    for (int h = h0 - 1; h <= h0 + 1; ++h) {
      if (h < 0 || h >= hops) {
        continue;
      }
      ++counted;
      const double period = track.periods[static_cast<std::size_t>(h)];
      if (period > 0.0) {
        voiced_periods.push_back(period);
      }
    }
    const bool voiced = static_cast<int>(voiced_periods.size()) * 2 > counted;
    out.push_back({.voiced = voiced,
                   .period = voiced ? dsp::Median(voiced_periods) : 0.0});
  }

  const auto n = static_cast<int>(out.size());
  std::vector<double> neighbours;
  for (int j = 0; j < n; ++j) {
    FrameVoicing& f = out[static_cast<std::size_t>(j)];
    if (!f.voiced) {
      continue;
    }
    neighbours.clear();
    for (const int i : std::array<int, 4>{j - kNeighbourReach, j - 1, j + 1,
                                          j + kNeighbourReach}) {
      if (i >= 0 && i < n && out[static_cast<std::size_t>(i)].voiced) {
        neighbours.push_back(out[static_cast<std::size_t>(i)].period);
      }
    }
    if (neighbours.size() < kMinimumNeighbours) {
      continue;
    }
    const double ratio = f.period / dsp::Median(neighbours);
    if (ratio > kHighRatio && kShortestPeriod <= f.period / 2.0) {
      f.period /= 2.0;
    } else if (ratio < kLowRatio && f.period * 2.0 <= kLongestPeriod) {
      f.period *= 2.0;
    }
  }
  return out;
}

void SmoothVoicingRuns(std::span<FrameVoicing> voicing, int min_run) {
  const auto n = static_cast<std::ptrdiff_t>(voicing.size());
  // Unvoiced islands first, so that a voiced island between two of them
  // is judged with them already filled.
  for (const bool island_voiced : {false, true}) {
    std::ptrdiff_t i = 0;
    while (i < n) {
      std::ptrdiff_t j = i;
      while (j < n && voicing[static_cast<std::size_t>(j)].voiced ==
                          voicing[static_cast<std::size_t>(i)].voiced) {
        ++j;
      }
      const bool inner = i > 0 && j < n;
      if (inner && j - i < min_run &&
          voicing[static_cast<std::size_t>(i)].voiced == island_voiced) {
        const double period =
            island_voiced
                ? 0.0
                : 0.5 * (voicing[static_cast<std::size_t>(i - 1)].period +
                         voicing[static_cast<std::size_t>(j)].period);
        for (std::ptrdiff_t k = i; k < j; ++k) {
          voicing[static_cast<std::size_t>(k)] = {.voiced = !island_voiced,
                                                  .period = period};
        }
      }
      i = j;
    }
  }
}

}  // namespace speak5220::core::encoder
