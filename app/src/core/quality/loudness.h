/*
 * File:        loudness.h
 * Module:      quality
 * Purpose:     The loudness figures of the chip's output: sounding RMS,
 *              peak, clipping, lattice wraps, and the level through the
 *              BBC Micro's speech path and A-weighted
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_LOUDNESS_H_
#define SPEAK5220_CORE_QUALITY_LOUDNESS_H_

#include <span>
#include <vector>

#include "core/lpc/frame.h"

namespace speak5220::core::quality {

// The largest code the DAC passes; codes beyond it clip (speak5220
// datasheet §8.9).
inline constexpr int kDacLimit = 127;

// The corner of the first-order high-pass of the BBC Micro's speech path
// (HLD §16).
inline constexpr double kBbcHighPassHz = 279.0;

// The sample from which the high-passed and A-weighted levels are taken:
// the output is idle for about the first frame (ReferenceSynthesis).
inline constexpr int kLoudnessStartSample = 200;

// The loudness figures of one phrase's output, in codes.
struct Loudness {
  // The RMS of the per-frame levels (encoder::MeasureOutputLevels()) over
  // the voiced, unvoiced and repeat frames: what the encoder's target_rms
  // aims at.
  double sounding_rms = 0.0;
  // The largest code magnitude.
  int peak = 0;
  // The fraction of samples (0 to 1) beyond +-kDacLimit, which the DAC
  // clips.
  double clipped = 0.0;
  // The samples in which a lattice sum wrapped (speak5220 datasheet §8.8).
  int wrapped_samples = 0;
  // The RMS from kLoudnessStartSample after a first-order high-pass at
  // kBbcHighPassHz: the level through a BBC Micro.
  double high_passed_rms = 0.0;
  // The A-weighted RMS from kLoudnessStartSample (IEC 61672-1 weighting
  // by the bilinear transform at 8 kHz, 0 dB at 1 kHz before it): a rough
  // loudness.
  double a_weighted_rms = 0.0;
};

// The loudness of `codes`, the chip's output for `frames` from the first
// frame read (synth::ReferenceSynthesizer, or Synth's VSM path), with
// `wrapped_samples` as the synthesis counted them. Re-implements the
// investigation's scratch measures (docs-tech/encoder-quality-
// investigation.md §3.3). Pure; thread-safe. O(samples).
Loudness MeasureLoudness(std::span<const int> codes,
                         std::span<const lpc::Frame> frames,
                         int wrapped_samples);

// `x` through the BBC Micro's high-pass: H(z) from the bilinear transform
// of a first-order high-pass at kBbcHighPassHz, prewarped, starting at
// rest (scipy.signal.lfilter). O(n).
std::vector<double> BbcHighPass(std::span<const double> x);

// `x` A-weighted: the analogue IEC 61672-1 weighting through
// scipy.signal.bilinear() at 8 kHz, as cascaded first-order sections,
// starting at rest. O(n).
std::vector<double> AWeighted(std::span<const double> x);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_LOUDNESS_H_
