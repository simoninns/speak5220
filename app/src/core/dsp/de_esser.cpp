/*
 * File:        de_esser.cpp
 * Module:      dsp
 * Purpose:     Turns down the hiss of sibilants (S, SH) in a recording: a
 *              split-band de-esser, which lowers only the treble band, and
 *              only where it is most of the sound
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/de_esser.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/dsp/envelope.h"

namespace speak5220::core::dsp {
namespace {

constexpr double kDecibelsPerDecade = 20.0;

}  // namespace

std::vector<double> DeEsserGain(std::span<const double> samples,
                                std::span<const double> band, int sample_rate,
                                double depth_db, const DeEsserShape& shape) {
  assert(band.size() == samples.size());
  const std::size_t n = samples.size();
  const std::vector<double> whole =
      CentredRms(samples, sample_rate, shape.window_ms);
  const std::vector<double> treble =
      CentredRms(band, sample_rate, shape.window_ms);
  std::vector<double> gain_db(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    if (whole[i] <= 0.0 || treble[i] <= 0.0) {
      continue;
    }
    const double share_db =
        kDecibelsPerDecade * std::log10(treble[i] / whole[i]);
    gain_db[i] = -std::clamp(share_db + depth_db, 0.0, depth_db);
  }
  const double step =
      shape.ramp_db_per_ms / static_cast<double>(SamplesOf(1.0, sample_rate));
  SpreadDips(gain_db, step, step);
  return DbToGain(gain_db);
}

std::vector<double> DeEss(std::span<const double> samples,
                          std::span<const double> band, int sample_rate,
                          double depth_db, const DeEsserShape& shape) {
  const std::vector<double> gain =
      DeEsserGain(samples, band, sample_rate, depth_db, shape);
  std::vector<double> out(samples.begin(), samples.end());
  for (std::size_t i = 0; i < out.size(); ++i) {
    if (gain[i] < 1.0) {
      out[i] -= (1.0 - gain[i]) * band[i];
    }
  }
  return out;
}

}  // namespace speak5220::core::dsp
