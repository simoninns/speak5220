/*
 * File:        pitch_estimator.cpp
 * Module:      quality
 * Purpose:     Pitch and voicing estimation for the quality measures: the
 *              interface and its SPTK implementation (REAPER or Harvest)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pitch_estimator.h"

#include <SPTK/analysis/pitch_extraction.h>

#include <algorithm>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace speak5220::core::quality {
namespace {

constexpr double kSampleRate = 8000.0;
// REAPER takes 16-bit samples; SPTK casts each value to an integer.
constexpr double kReaperScale = 32767.0;
// The defaults of the original tools: REAPER's unvoiced cost and
// Harvest's allowed range, which SPTK takes as its voicing threshold.
constexpr double kReaperUnvoicedCost = 0.9;
constexpr double kHarvestAllowedRange = 0.18;

}  // namespace

std::string_view PitchAlgorithmName(PitchAlgorithm algorithm) {
  switch (algorithm) {
    case PitchAlgorithm::kReaper:
      return "REAPER";
    case PitchAlgorithm::kHarvest:
      return "Harvest";
  }
  return "REAPER";
}

SptkPitchEstimator::SptkPitchEstimator(PitchAlgorithm algorithm)
    : algorithm_(algorithm) {}

StatusOr<std::vector<double>> SptkPitchEstimator::Track(
    std::span<const double> samples) {
  if (samples.empty()) {
    return std::vector<double>();
  }
  const bool reaper = algorithm_ == PitchAlgorithm::kReaper;
  const sptk::PitchExtraction extraction(
      kPitchHop, kSampleRate, kLowestF0, kHighestF0,
      reaper ? kReaperUnvoicedCost : kHarvestAllowedRange,
      reaper ? sptk::PitchExtraction::kReaper
             : sptk::PitchExtraction::kHarvest);
  if (!extraction.IsValid()) {
    return InternalError("SPTK: cannot set up the pitch estimator");
  }
  std::vector<double> waveform(samples.begin(), samples.end());
  if (reaper) {
    for (double& v : waveform) {
      v = std::clamp(v * kReaperScale, -kReaperScale, kReaperScale);
    }
  }
  std::vector<double> f0;
  if (!extraction.Run(waveform, &f0, nullptr, nullptr)) {
    return FailedPreconditionError(std::string(PitchAlgorithmName(algorithm_)) +
                                   " cannot track the signal");
  }
  return f0;
}

}  // namespace speak5220::core::quality
