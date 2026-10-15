/*
 * File:        anchor_vocoder.h
 * Module:      quality
 * Purpose:     The anchor: an unquantised floating-point LPC-10 vocoder of
 *              a phrase's source, what unquantised LPC reaches, for the
 *              calibration of the quality score
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_ANCHOR_VOCODER_H_
#define SPEAK5220_CORE_QUALITY_ANCHOR_VOCODER_H_

#include <array>
#include <span>
#include <vector>

#include "core/quality/numpy_normal.h"

namespace speak5220::core::quality {

// The best a TMS5220-style LPC coding could do with a source,
// with nothing quantised, which places the good ends of the quality
// score's calibration. Order 10, 25 ms
// frames, K and gain interpolated over 8 steps a frame, an impulse per
// pitch period or Gaussian noise, pre- and de-emphasis 0.9375. Re-implements
// investigation/tools/speechqual.py lpcVocoder() with its noise from
// numpy.random.default_rng(1).

inline constexpr int kAnchorOrder = 10;

// Per 10 ms analysis frame: the pitch (Hz) and whether it is voiced.
struct AnchorPitch {
  std::vector<double> f0;
  std::vector<bool> voiced;
};

// speechqual.py pitch(): normalised cross-correlation of the signal
// low-passed at 1 kHz over lags of 50-400 Hz, preferring a third or half
// of the best lag within 10% of it, refined parabolically; voiced at a
// correlation of 0.5. O(n) with a large constant.
AnchorPitch TrackAnchorPitch(std::span<const double> x);

// One 25 ms frame of the anchor's analysis.
struct AnchorFrame {
  // Reflection coefficients, the chip's sign (k1 = -R1/R0).
  std::array<double, kAnchorOrder> k{};
  // The prediction residual's RMS.
  double gain = 0.0;
  // The pitch period in samples; 0 when unvoiced.
  double period = 0.0;
};

// speechqual.py lpcAnalysis(): pre-emphasised, Hamming-windowed 30 ms
// frames every 25 ms, a 60 Hz Gaussian lag window, Levinson to order 10,
// and the median pitch of the voiced speech frames around each centre.
std::vector<AnchorFrame> AnalyseAnchor(std::span<const double> x);

// speechqual.py synthLinear() with a pulse excitation: the frames
// rendered to `n` samples, de-emphasised, with the half-frame lag
// removed. Draws one value from `noise` per unvoiced sample.
std::vector<double> SynthesiseAnchor(std::span<const AnchorFrame> frames, int n,
                                     NumpyNormal& noise);

// The anchor vocoder's output for `x`: AnalyseAnchor(), then
// SynthesiseAnchor() with numpy.random.default_rng(1)'s noise. Same
// length as x. Deterministic; thread-safe.
std::vector<double> AnchorVocoder(std::span<const double> x);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_ANCHOR_VOCODER_H_
