/*
 * File:        encoder_filters.h
 * Module:      encoder
 * Purpose:     The encoder's fixed filters: the 60 Hz high-pass, the 1 kHz
 *              low-pass, the analysis pre-emphasis and the loudness
 *              high-pass
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_ENCODER_FILTERS_H_
#define SPEAK5220_CORE_ENCODER_ENCODER_FILTERS_H_

#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::encoder {

// Zero-phase order-2 Butterworth high-pass at 60 Hz, which removes rumble
// before analysis. Inputs under 64 samples are returned unchanged.
// Re-implements tms5220enc.py _sosfiltfilt(_butter("highpass", 60.0, 2)).
std::vector<double> HighPass60Hz(std::span<const double> x);

// Zero-phase order-4 Butterworth low-pass at 1 kHz, the band the pitch
// tracker uses. Inputs under 64 samples are returned unchanged.
// Re-implements tms5220enc.py _sosfiltfilt(_butter("lowpass", 1000.0)).
std::vector<double> LowPass1kHz(std::span<const double> x);

// The FIR that whitens the chirp, the voiced analysis's pre-emphasis: the
// order-4 LPC inverse of the chirp ROM's 21 non-zero samples (speak5220
// datasheet §8.1, Table 8-7).
// Re-implements tms5220enc.py chirpInverse().
std::vector<double> ChirpInverse();

// The corner of the first-order high-pass of the BBC Micro's speech path
// (C21/R31; docs/technical/hardware/audio-output-stage.md), through which
// the encoder measures loudness.
inline constexpr double kLoudnessHighPassHz = 279.0;

// `x` (8 kHz) through a first-order high-pass at kLoudnessHighPassHz: the
// bilinear transform, prewarped, starting at rest, as
// scipy.signal.lfilter() would. The levels the encoder aims at and
// measures are taken after it, so bass does not count as loudness. O(n).
std::vector<double> LoudnessHighPass(std::span<const double> x);

// The power gain |H|^2 of LoudnessHighPass() at each of the `bins` bins of
// a real FFT of size 2 (bins - 1), from 0 Hz to half the sample rate.
std::vector<double> LoudnessHighPassPower(std::size_t bins);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_ENCODER_FILTERS_H_
