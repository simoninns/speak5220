/*
 * File:        normaliser.cpp
 * Module:      dsp
 * Purpose:     Brings a recording's speech to a set loudness and evens it
 *              out, as broadcast processing does: speech-gated loudness, a
 *              soft-knee compressor, and a lookahead peak limiter
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/normaliser.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "core/dsp/envelope.h"

namespace speak5220::core::dsp {
namespace {

constexpr double kDecibelsPerDecade = 20.0;
constexpr double kPowerDecibelsPerDecade = 10.0;

// The gain change, in dB (0 or less), the compressor's soft-knee curve
// gives a level `over_db` above the threshold (Giannoulis, Massberg and
// Reiss, 2012).
double CompressorDb(double over_db, double ratio, double knee_db) {
  const double slope = (1.0 / ratio) - 1.0;
  if (2.0 * over_db <= -knee_db) {
    return 0.0;
  }
  if (2.0 * over_db < knee_db) {
    const double into = over_db + (knee_db / 2.0);
    return slope * into * into / (2.0 * knee_db);
  }
  return slope * over_db;
}

// `samples` times the gain 10^(gain_db / 20).
std::vector<double> Scaled(std::span<const double> samples,
                           std::span<const double> gain_db) {
  const std::vector<double> gain = DbToGain(gain_db);
  std::vector<double> out(samples.size());
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] = samples[i] * gain[i];
  }
  return out;
}

}  // namespace

std::optional<double> SpeechLoudness(std::span<const double> samples,
                                     int sample_rate,
                                     const NormaliserShape& shape) {
  const std::size_t n = samples.size();
  if (n == 0) {
    return std::nullopt;
  }
  std::vector<double> energy(n + 1, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    energy[i + 1] = energy[i] + (samples[i] * samples[i]);
  }
  const std::size_t block = std::min(SamplesOf(shape.block_ms, sample_rate), n);
  const std::size_t hop = SamplesOf(shape.hop_ms, sample_rate);
  const double absolute_gate =
      std::pow(10.0, shape.absolute_gate_db / kPowerDecibelsPerDecade);
  std::vector<double> powers;
  for (std::size_t begin = 0; begin + block <= n; begin += hop) {
    const double power = std::max(energy[begin + block] - energy[begin], 0.0) /
                         static_cast<double>(block);
    if (power > absolute_gate) {
      powers.push_back(power);
    }
  }
  if (powers.empty()) {
    return std::nullopt;
  }
  double sum = 0.0;
  for (const double power : powers) {
    sum += power;
  }
  const double relative_gate =
      sum / static_cast<double>(powers.size()) *
      std::pow(10.0, shape.relative_gate_db / kPowerDecibelsPerDecade);
  double gated_sum = 0.0;
  std::size_t gated = 0;
  for (const double power : powers) {
    if (power > relative_gate) {
      gated_sum += power;
      ++gated;
    }
  }
  // The loudest block always passes the relative gate.
  return kPowerDecibelsPerDecade *
         std::log10(gated_sum / static_cast<double>(gated));
}

std::vector<double> NormaliserGain(std::span<const double> samples,
                                   int sample_rate, double target_db,
                                   double ratio, const NormaliserShape& shape) {
  const std::size_t n = samples.size();
  const std::optional<double> loudness =
      SpeechLoudness(samples, sample_rate, shape);
  if (!loudness.has_value()) {
    std::vector<double> unity(n, 1.0);
    return unity;
  }

  // To the target, then the compressor on what is louder than its
  // threshold.
  const double to_target_db = target_db - *loudness;
  const double threshold_db = target_db - shape.threshold_below_db;
  std::vector<double> gain_db(n, 0.0);
  if (ratio > 1.0) {
    const std::vector<double> level =
        CentredRms(samples, sample_rate, shape.window_ms);
    for (std::size_t i = 0; i < n; ++i) {
      if (level[i] > 0.0) {
        const double level_db =
            (kDecibelsPerDecade * std::log10(level[i])) + to_target_db;
        gain_db[i] =
            CompressorDb(level_db - threshold_db, ratio, shape.knee_db);
      }
    }
    const auto per_ms = static_cast<double>(SamplesOf(1.0, sample_rate));
    SpreadDips(gain_db, shape.release_db_per_ms / per_ms,
               shape.attack_db_per_ms / per_ms);
  }
  for (double& value : gain_db) {
    value += to_target_db;
  }

  // Make-up gain: back to the target after the compressor.
  if (const std::optional<double> compressed =
          SpeechLoudness(Scaled(samples, gain_db), sample_rate, shape);
      compressed.has_value()) {
    for (double& value : gain_db) {
      value += target_db - *compressed;
    }
  }

  // The limiter: no sample's peak over the ceiling.
  std::vector<double> limit_db(n, 0.0);
  for (std::size_t i = 0; i < n; ++i) {
    const double peak = std::abs(samples[i]);
    if (peak > 0.0) {
      const double peak_db =
          (kDecibelsPerDecade * std::log10(peak)) + gain_db[i];
      limit_db[i] = std::min(0.0, shape.ceiling_db - peak_db);
    }
  }
  const auto per_ms = static_cast<double>(SamplesOf(1.0, sample_rate));
  SpreadDips(limit_db, shape.limiter_release_db_per_ms / per_ms,
             shape.limiter_attack_db_per_ms / per_ms);
  for (std::size_t i = 0; i < n; ++i) {
    gain_db[i] += limit_db[i];
  }
  return DbToGain(gain_db);
}

std::vector<double> Normalise(std::span<const double> samples, int sample_rate,
                              double target_db, double ratio,
                              const NormaliserShape& shape) {
  std::vector<double> out =
      NormaliserGain(samples, sample_rate, target_db, ratio, shape);
  for (std::size_t i = 0; i < out.size(); ++i) {
    out[i] *= samples[i];
  }
  return out;
}

}  // namespace speak5220::core::dsp
