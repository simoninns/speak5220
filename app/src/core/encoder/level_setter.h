/*
 * File:        level_setter.h
 * Module:      encoder
 * Purpose:     Sets each frame's energy: level shaping, an open-loop
 *              estimate, closed-loop passes and the peak limiter
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_LEVEL_SETTER_H_
#define SPEAK5220_CORE_ENCODER_LEVEL_SETTER_H_

#include <complex>
#include <span>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"

namespace speak5220::core::encoder {

// Sets the frames' energies. Implementations are stateless between calls.
class ILevelSetter {
 public:
  virtual ~ILevelSetter() = default;

  // The frame levels after the optional slow gain control
  // (options.level_agc), on sounding frames only.
  virtual std::vector<double> ShapeLevels(std::span<const double> levels,
                                          std::span<const PlanEntry> plan,
                                          const EncoderOptions& options) = 0;

  // One energy index per plan entry (0 for silence), aiming each sounding
  // frame's output level at levels[j] x gain codes, with no output code
  // beyond 120 and no lattice wrap. `stats`, if not null, receives what the
  // limiter changed.
  virtual std::vector<int> SetLevels(std::span<const PlanEntry> plan,
                                     std::span<const double> levels,
                                     double gain, LevelStats* stats) = 0;
};

// The output RMS in codes of the filter of reflection coefficients
// `k_values` at energy value 1 (speak5220 datasheet §12.3: code =
// E x h / 128): white excitation of RMS 64 for period 0, else the chirp
// every `period` samples. Re-implements tms5220enc.py _gainOf().
// O(4096 log 4096).
double GainOf(dsp::IFft& fft, std::span<const double> k_values, int period);

// The K values in force at each frame for the open-loop gain: those of the
// last full frame, 10 for a voiced frame after a voiced full frame, else 4;
// empty for silence. Re-implements tms5220enc.py _effectiveK().
std::vector<std::vector<double>> EffectiveK(std::span<const PlanEntry> plan);

// The output level (as FrameRms()) and peak around each frame's centre in
// `codes`; the peak covers peak_half samples either side. Re-implements
// tms5220enc.py _outLevels().
struct OutputLevels {
  std::vector<double> level;
  std::vector<int> peak;
};
OutputLevels MeasureOutputLevels(std::span<const int> codes, int frames,
                                 int peak_half = 100);

// ILevelSetter as tms5220enc.py _shapeLevels() and _setLevels() with its
// default iterations, peakLimit and compression: an open-loop energy from
// each frame's gain; up to five closed-loop passes through the reference
// synthesiser, moving each frame 70% of the way to its target in dB and
// keeping the best pass; frames far too loud even at energy 1 made silent;
// then a limiter that lowers frames whose peak is over 120, and a safety
// net that lowers everything until nothing wraps. Thread-safe if the IFft
// is.
class LevelSetter : public ILevelSetter {
 public:
  // `fft` must outlive the level setter.
  explicit LevelSetter(dsp::IFft& fft);

  std::vector<double> ShapeLevels(std::span<const double> levels,
                                  std::span<const PlanEntry> plan,
                                  const EncoderOptions& options) override;
  std::vector<int> SetLevels(std::span<const PlanEntry> plan,
                             std::span<const double> levels, double gain,
                             LevelStats* stats) override;

 private:
  dsp::IFft* fft_;
};

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_LEVEL_SETTER_H_
