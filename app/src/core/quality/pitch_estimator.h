/*
 * File:        pitch_estimator.h
 * Module:      quality
 * Purpose:     Pitch and voicing estimation for the quality measures and
 *              the encoder's pitch anchor: the interface and its SPTK
 *              implementation (REAPER, Harvest or RAPT)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_PITCH_ESTIMATOR_H_
#define SPEAK5220_CORE_QUALITY_PITCH_ESTIMATOR_H_

#include <span>
#include <string_view>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::quality {

// The pitch frame: 10 ms at 8 kHz.
inline constexpr int kPitchHop = 80;

// Estimates F0 and voicing. Implementations are thread-safe.
class IPitchEstimator {
 public:
  virtual ~IPitchEstimator() = default;

  // The F0 in Hz of `samples` (8 kHz, -1..1) at each multiple of
  // kPitchHop, ceil(size / kPitchHop) values; 0 where the signal is
  // unvoiced.
  virtual StatusOr<std::vector<double>> Track(
      std::span<const double> samples) = 0;
};

// The estimators SPTK 4 provides, each SPTK's own copy of the
// original code.
enum class PitchAlgorithm {
  // Google's REAPER, with its defaults: an 80 Hz high-pass, no Hilbert
  // transform and an unvoiced cost of 0.9.
  kReaper,
  // WORLD's Harvest, with the allowed range the original fixes (0.18,
  // in its FixStep3()), which SPTK takes as its voicing threshold.
  kHarvest,
  // Talkin's RAPT (Snack's get_f0, made reentrant by SPTK), with SPTK's
  // default voicing threshold of 0. SPTK adds Gaussian noise of standard
  // deviation 50 on the 16-bit scale, from a fixed seed, before tracking.
  kRapt,
};

// The algorithm's name, as the GUI and logs show it.
std::string_view PitchAlgorithmName(PitchAlgorithm algorithm);

// IPitchEstimator over SPTK's PitchExtraction, searching F0 between
// kLowestF0 and kHighestF0. Stateless; thread-safe.
class SptkPitchEstimator : public IPitchEstimator {
 public:
  // The F0 search range, in Hz.
  static constexpr double kLowestF0 = 50.0;
  static constexpr double kHighestF0 = 400.0;

  explicit SptkPitchEstimator(PitchAlgorithm algorithm);

  StatusOr<std::vector<double>> Track(std::span<const double> samples) override;

 private:
  PitchAlgorithm algorithm_;
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_PITCH_ESTIMATOR_H_
