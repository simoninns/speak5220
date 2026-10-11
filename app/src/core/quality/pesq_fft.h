/*
 * File:        pesq_fft.h
 * Module:      quality
 * Purpose:     The single-precision radix-2 FFT that PESQ's reference
 *              implementation computes with, in its order of operations
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_PESQ_FFT_H_
#define SPEAK5220_CORE_QUALITY_PESQ_FFT_H_

#include <span>
#include <vector>

namespace speak5220::core::quality {

// PESQ's scores rest on many threshold decisions (voice activity, delay
// histograms, bad frames), so this FFT reproduces the reference
// implementation's float arithmetic step for step rather than using
// pocketfft: an in-place decimation-in-frequency radix-2 transform with
// single-precision twiddle factors, then a bit-reversal swap. Sizes must be
// powers of two. All functions are pure and thread-safe; O(n log n).
// ITU-T P.862 Annex A: the ANSI-C reference implementation (dsp.c) takes
// precedence over the Recommendation's text.

// The forward transform of `n` complex values, interleaved (re, im), in
// place. `x` holds 2n floats.
void ComplexFft(std::span<float> x, int n);

// The inverse transform, scaled by 1/n, in place.
void ComplexIfft(std::span<float> x, int n);

// The forward transform of `n` real values in x[0..n). On return x holds
// the n/2 + 1 complex values of the half spectrum, interleaved, so `x`
// must hold at least n + 2 floats.
void RealFft(std::span<float> x, int n);

// The inverse of RealFft(): reads the n/2 + 1 complex values and writes
// the n real values to x[0..n).
void RealIfft(std::span<float> x, int n);

// The smallest power of two at or above `x` (1 for 0).
int NextPowerOfTwo(int x);

// The full cross-correlation of x1 and x2 by FFT, of length
// x1.size() + x2.size() - 1, as the reference's FFTNXCorr(): entry
// x1.size() - 1 is the zero lag, and later entries are x2 delayed.
std::vector<float> CrossCorrelate(std::span<const float> x1,
                                  std::span<const float> x2);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_PESQ_FFT_H_
