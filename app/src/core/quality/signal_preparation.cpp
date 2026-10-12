/*
 * File:        signal_preparation.cpp
 * Module:      quality
 * Purpose:     Prepares an original and the chip's output alike for the
 *              quality measures: placement, band-limit, voice activity
 *              mask and level
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/signal_preparation.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/filter_constants.h"
#include "core/dsp/iir_filter.h"
#include "core/synth/synth.h"
#include "core/util/logging.h"

namespace speak5220::core::quality {
namespace {

// Full scale of a 16-bit sample.
constexpr double kFullScale = 32768.0;
// The chip's idle code, as a sample.
constexpr int kIdleSample = -synth::kSampleScale;

}  // namespace

std::vector<double> PlaceOutput(std::span<const std::int16_t> samples,
                                int source_offset, int n) {
  std::vector<double> placed(static_cast<std::size_t>(std::max(n, 0)), 0.0);
  for (int s = 0; s < n; ++s) {
    const std::int64_t i = static_cast<std::int64_t>(s) + source_offset;
    if (i >= 0 && std::cmp_less(i, samples.size())) {
      placed[static_cast<std::size_t>(s)] =
          (samples[static_cast<std::size_t>(i)] - kIdleSample) / kFullScale;
    }
  }
  return placed;
}

std::vector<double> BandLimit(std::span<const double> x) {
  return dsp::SosFiltFilt(dsp::kQualityBand, x);
}

StatusOr<std::vector<bool>> SpeechMask(std::span<const double> original,
                                       IVoiceActivity& vad) {
  StatusOr<std::vector<bool>> detected = vad.SpeechFrames(original);
  if (!detected.IsOk()) {
    return detected.GetStatus();
  }
  const std::size_t frames = original.size() / kVoiceActivityFrame;
  std::vector<bool> speech = std::move(*detected);
  speech.resize(frames, false);
  std::vector<double> power(frames);
  for (std::size_t f = 0; f < frames; ++f) {
    double sum = 0.0;
    for (const double v :
         original.subspan(f * kVoiceActivityFrame, kVoiceActivityFrame)) {
      sum += v * v;
    }
    power[f] = sum / kVoiceActivityFrame;
  }
  const double loudest = power.empty() ? 0.0 : *std::ranges::max_element(power);
  const double floor = loudest * std::pow(10.0, -kMaskFloorDb / 10.0);
  for (std::size_t f = 0; f < frames; ++f) {
    speech[f] = speech[f] && loudest > 0.0 && power[f] > floor;
  }
  return speech;
}

double MaskedLevelGain(std::span<const double> original,
                       std::span<const double> output,
                       const std::vector<bool>& speech) {
  double original_power = 0.0;
  double output_power = 0.0;
  const std::size_t n = std::min(original.size(), output.size());
  for (std::size_t i = 0; i < n; ++i) {
    const std::size_t f = i / kVoiceActivityFrame;
    if (f < speech.size() && speech[f]) {
      original_power += original[i] * original[i];
      output_power += output[i] * output[i];
    }
  }
  if (output_power <= 0.0) {
    return 1.0;
  }
  return std::sqrt(original_power / output_power);
}

StatusOr<PreparedSignals> PrepareAlignedSignals(std::span<const double> source,
                                                std::span<const double> output,
                                                IVoiceActivity& vad) {
  PreparedSignals prepared;
  prepared.original = BandLimit(source);
  prepared.output = BandLimit(output);
  prepared.output.resize(prepared.original.size(), 0.0);
  StatusOr<std::vector<bool>> speech = SpeechMask(prepared.original, vad);
  if (!speech.IsOk()) {
    return speech.GetStatus();
  }
  prepared.speech = std::move(*speech);
  const double gain =
      MaskedLevelGain(prepared.original, prepared.output, prepared.speech);
  for (double& v : prepared.output) {
    v *= gain;
  }
  Logger(kLogModuleQuality)
      ->debug("prepared {} samples: {} of {} frames speech, output gain {:.3f}",
              source.size(), std::ranges::count(prepared.speech, true),
              prepared.speech.size(), gain);
  return prepared;
}

StatusOr<PreparedSignals> PrepareSignals(std::span<const double> source,
                                         const EncodedAudio& audio,
                                         IVoiceActivity& vad) {
  const std::vector<double> placed = PlaceOutput(
      audio.samples, audio.source_offset, static_cast<int>(source.size()));
  return PrepareAlignedSignals(source, placed, vad);
}

}  // namespace speak5220::core::quality
