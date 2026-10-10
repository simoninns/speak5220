/*
 * File:        encoder_filters.h
 * Module:      encoder
 * Purpose:     The encoder's fixed filters: the 60 Hz high-pass, the 1 kHz
 *              low-pass and the analysis pre-emphasis
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_ENCODER_FILTERS_H_
#define SPEAK5220_CORE_ENCODER_ENCODER_FILTERS_H_

#include <span>
#include <vector>

#include "core/encoder/encoder_options.h"

namespace speak5220::core::encoder {

// Zero-phase order-2 Butterworth high-pass at 60 Hz, which removes rumble
// before analysis. Inputs under 64 samples are returned unchanged.
// Re-implements tms5220enc.py _sosfiltfilt(_butter("highpass", 60.0, 2)).
std::vector<double> HighPass60Hz(std::span<const double> x);

// Zero-phase order-4 Butterworth low-pass at 1 kHz, the band the pitch
// tracker uses. Inputs under 64 samples are returned unchanged.
// Re-implements tms5220enc.py _sosfiltfilt(_butter("lowpass", 1000.0)).
std::vector<double> LowPass1kHz(std::span<const double> x);

// The FIR that whitens the chirp: the order-4 LPC inverse of the chirp
// ROM's 21 non-zero samples (speak5220 datasheet §8.1, Table 8-7).
// Re-implements tms5220enc.py chirpInverse().
std::vector<double> ChirpInverse();

// The pre-emphasis FIR for `pre_emphasis`: ChirpInverse(), or 1 - mu z^-1.
// Re-implements tms5220enc.py preEmphasisFilter().
std::vector<double> PreEmphasisFilter(const PreEmphasis& pre_emphasis);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_ENCODER_FILTERS_H_
