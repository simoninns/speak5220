/*
 * File:        pitch_estimator.cpp
 * Module:      quality
 * Purpose:     Pitch and voicing estimation for the quality measures and
 *              the encoder's pitch anchor: the interface and its SPTK
 *              implementation (REAPER, Harvest or RAPT)
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
// REAPER and RAPT take 16-bit samples: SPTK casts REAPER's each to an
// integer, and adds RAPT's noise on that scale.
constexpr double kSixteenBitScale = 32767.0;
// The defaults of the original tools: REAPER's unvoiced cost and
// Harvest's allowed range, which SPTK takes as its voicing threshold; and
// SPTK's own for RAPT.
constexpr double kReaperUnvoicedCost = 0.9;
constexpr double kHarvestAllowedRange = 0.18;
constexpr double kRaptVoicingThreshold = 0.0;

}  // namespace

std::string_view PitchAlgorithmName(PitchAlgorithm algorithm) {
  switch (algorithm) {
    case PitchAlgorithm::kReaper:
      return "REAPER";
    case PitchAlgorithm::kHarvest:
      return "Harvest";
    case PitchAlgorithm::kRapt:
      return "RAPT";
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
  const bool rapt = algorithm_ == PitchAlgorithm::kRapt;
  double threshold = kHarvestAllowedRange;
  sptk::PitchExtraction::Algorithms method = sptk::PitchExtraction::kHarvest;
  if (reaper) {
    threshold = kReaperUnvoicedCost;
    method = sptk::PitchExtraction::kReaper;
  } else if (rapt) {
    threshold = kRaptVoicingThreshold;
    method = sptk::PitchExtraction::kRapt;
  }
  const sptk::PitchExtraction extraction(kPitchHop, kSampleRate, kLowestF0,
                                         kHighestF0, threshold, method);
  if (!extraction.IsValid()) {
    return InternalError("SPTK: cannot set up the pitch estimator");
  }
  std::vector<double> waveform(samples.begin(), samples.end());
  if (reaper || rapt) {
    for (double& v : waveform) {
      v = std::clamp(v * kSixteenBitScale, -kSixteenBitScale, kSixteenBitScale);
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
