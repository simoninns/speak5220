/*
 * File:        preprocess.cpp
 * Module:      preprocess
 * Purpose:     The preprocessing of a phrase's recording before it is
 *              encoded: the noise gate, the bass cut, the treble tilt and
 *              the de-esser, and their presets
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/preprocess/preprocess.h"

#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

#include "core/dsp/de_esser.h"
#include "core/dsp/iir_filter.h"
#include "core/dsp/noise_gate.h"

namespace speak5220::core::preprocess {
namespace {

// scipy.signal.sosfiltfilt()'s default padding for one section:
// 3 x (2 x sections + 1).
constexpr int kOneSectionPad = 9;

// HLD §9: Acorn's PHROM phrases have about 11 dB less at 125 Hz and up to
// 5 dB more treble than the recordings of the corpus; this pair fits that
// difference within 3.7 dB rms.
constexpr double kBbcMicroBassCutHz = 200.0;
constexpr double kBbcMicroTrebleTilt = 0.4;

constexpr std::array<PreprocessPreset, 2> kPresets = {{
    {.name = "None", .options = {}},
    {.name = "BBC Micro",
     .options = {.bass_cut_hz = kBbcMicroBassCutHz,
                 .treble_tilt = kBbcMicroTrebleTilt}},
}};

}  // namespace

std::span<const PreprocessPreset> PreprocessPresets() { return kPresets; }

const PreprocessPreset* MatchingPreset(const PreprocessOptions& options) {
  // The gate's speed does nothing while the gate is off.
  PreprocessOptions effective = options;
  if (!effective.noise_gate_db.has_value()) {
    effective.noise_gate_ms = kDefaultNoiseGateMs;
  }
  for (const PreprocessPreset& preset : kPresets) {
    if (preset.options == effective) {
      return &preset;
    }
  }
  return nullptr;
}

PreprocessedAudio Preprocess(std::span<const double> original,
                             const PreprocessOptions& options) {
  PreprocessedAudio audio;
  audio.voicing =
      options.noise_gate_db.has_value()
          ? dsp::NoiseGate(original, kPreprocessSampleRate,
                           *options.noise_gate_db, options.noise_gate_ms)
          : std::vector<double>(original.begin(), original.end());
  audio.samples = audio.voicing;
  if (options.bass_cut_hz.has_value() &&
      audio.samples.size() > static_cast<std::size_t>(kOneSectionPad)) {
    const std::array<dsp::SosSection, 1> sections = {
        ButterworthHighPass(*options.bass_cut_hz)};
    audio.samples = dsp::SosFiltFilt(sections, kOneSectionPad, audio.samples);
  }
  if (options.treble_tilt > 0.0) {
    const std::array<double, 2> tilt = {1.0, -options.treble_tilt};
    audio.samples = dsp::FirFilter(tilt, audio.samples);
  }
  if (options.de_ess_db.has_value() &&
      audio.samples.size() > static_cast<std::size_t>(kOneSectionPad)) {
    const std::array<dsp::SosSection, 1> sections = {
        ButterworthHighPass(kDeEssBandHz)};
    const std::vector<double> band =
        dsp::SosFiltFilt(sections, kOneSectionPad, audio.samples);
    audio.samples = dsp::DeEss(audio.samples, band, kPreprocessSampleRate,
                               *options.de_ess_db);
  }
  return audio;
}

std::vector<dsp::SampleSpan> GatedSpans(std::span<const double> original,
                                        const PreprocessOptions& options) {
  if (!options.noise_gate_db.has_value()) {
    return {};
  }
  return dsp::GatedSpans(original, kPreprocessSampleRate,
                         *options.noise_gate_db, options.noise_gate_ms);
}

dsp::SosSection ButterworthHighPass(double cutoff_hz) {
  const double k =
      std::tan(std::numbers::pi * cutoff_hz / kPreprocessSampleRate);
  const double norm = 1.0 / (1.0 + (std::numbers::sqrt2 * k) + (k * k));
  dsp::SosSection section;
  section.b = {norm, -2.0 * norm, norm};
  section.a = {2.0 * ((k * k) - 1.0) * norm,
               (1.0 - (std::numbers::sqrt2 * k) + (k * k)) * norm};
  // A unit step's steady state: the output is 0 (no gain at DC), so the
  // two delays hold -b0 and b2 (scipy.signal.sosfilt_zi()).
  section.zi = {-section.b[0], section.b[2]};
  return section;
}

}  // namespace speak5220::core::preprocess
