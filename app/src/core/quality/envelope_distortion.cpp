/*
 * File:        envelope_distortion.cpp
 * Module:      quality
 * Purpose:     The spectral envelope measure: log spectral distortion
 *              between the LPC envelopes of an original and the chip's
 *              output
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/envelope_distortion.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/util/logging.h"

namespace speak5220::core::quality {
namespace {

constexpr double kSampleRate = 8000.0;

// The envelope shape of `frame` (windowed): its fit with the gain set to 1,
// or flat (no fit) when the frame is silent. `silent` says which.
StatusOr<std::vector<double>> ShapeDb(std::span<const double> frame,
                                      ILpcAnalysis& lpc, bool& silent) {
  StatusOr<LpcFit> fit = lpc.Analyse(frame, kEnvelopeOrder);
  silent = !fit.IsOk();
  if (silent && fit.GetStatus().Code() != StatusCode::kFailedPrecondition) {
    return fit.GetStatus();
  }
  LpcFit shape{.gain = 1.0,
               .a = silent ? std::vector<double>(kEnvelopeOrder, 0.0)
                           : std::move(fit->a)};
  return lpc.EnvelopeDb(shape, kEnvelopeFft);
}

}  // namespace

double LogSpectralDistortion(std::span<const double> a_db,
                             std::span<const double> b_db, int first,
                             int last) {
  double sum = 0.0;
  for (int k = first; k <= last; ++k) {
    const double d =
        a_db[static_cast<std::size_t>(k)] - b_db[static_cast<std::size_t>(k)];
    sum += d * d;
  }
  return std::sqrt(sum / static_cast<double>(last - first + 1));
}

StatusOr<EnvelopeDistortion> MeasureEnvelopeDistortion(
    const PreparedSignals& prepared, ILpcAnalysis& lpc) {
  // The grid's bins within the band compared.
  const double bin_hz = kSampleRate / kEnvelopeFft;
  const int first = static_cast<int>(std::ceil(kEnvelopeLowHz / bin_hz));
  const int last = static_cast<int>(std::floor(kEnvelopeHighHz / bin_hz));
  const std::vector<double> window = dsp::HammingWindow(kEnvelopeFrame);

  std::vector<double> distortions;
  const std::size_t frames = prepared.original.size() / kEnvelopeFrame;
  std::vector<double> x(kEnvelopeFrame);
  std::vector<double> y(kEnvelopeFrame);
  for (std::size_t k = 0; k < frames; ++k) {
    const std::size_t start = k * kEnvelopeFrame;
    const std::size_t centre =
        (start + kEnvelopeFrame / 2) / kVoiceActivityFrame;
    if (centre >= prepared.speech.size() || !prepared.speech[centre]) {
      continue;
    }
    for (std::size_t i = 0; i < kEnvelopeFrame; ++i) {
      x[i] = prepared.original[start + i] * window[i];
      y[i] = prepared.output[start + i] * window[i];
    }
    bool silent = false;
    StatusOr<std::vector<double>> original = ShapeDb(x, lpc, silent);
    if (!original.IsOk()) {
      return original.GetStatus();
    }
    if (silent) {
      continue;
    }
    StatusOr<std::vector<double>> output = ShapeDb(y, lpc, silent);
    if (!output.IsOk()) {
      return output.GetStatus();
    }
    distortions.push_back(
        LogSpectralDistortion(*original, *output, first, last));
  }

  EnvelopeDistortion result;
  result.frames = static_cast<int>(distortions.size());
  if (distortions.empty()) {
    return result;
  }
  int over_2db = 0;
  int over_4db = 0;
  double sum = 0.0;
  for (const double d : distortions) {
    sum += d;
    over_2db += d > kOutlierDb ? 1 : 0;
    over_4db += d > kLargeOutlierDb ? 1 : 0;
  }
  const auto n = static_cast<double>(distortions.size());
  result.mean_db = sum / n;
  result.over_2db = 100.0 * over_2db / n;
  result.over_4db = 100.0 * over_4db / n;
  Logger(kLogModuleQuality)
      ->debug(
          "envelope: {:.2f} dB over {} frames, {:.1f}% over 2 dB, "
          "{:.1f}% over 4 dB",
          result.mean_db, result.frames, result.over_2db, result.over_4db);
  return result;
}

}  // namespace speak5220::core::quality
