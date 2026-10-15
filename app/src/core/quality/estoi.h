/*
 * File:        estoi.h
 * Module:      quality
 * Purpose:     The intelligibility measure: extended short-time objective
 *              intelligibility (ESTOI), a port of pystoi's
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_ESTOI_H_
#define SPEAK5220_CORE_QUALITY_ESTOI_H_

#include <array>
#include <span>
#include <utility>
#include <vector>

#include "core/quality/signal_preparation.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// Jensen and Taal's ESTOI as pystoi 0.4.1 computes it (pystoi/stoi.py,
// stoi(..., extended=True)): both signals resampled to 10 kHz, the frames
// of the original more than 40 dB below its loudest removed from both,
// one-third-octave band envelopes of 256-sample Hann frames, and the mean
// over 384 ms segments of the correlation of their row- and
// column-normalised envelopes. Re-implements pystoi's stoi(),
// resample_oct(), remove_silent_frames(), stft(), thirdoct() and
// row_col_normalize(); each function names its original.
//
// J. Jensen and C. H. Taal, "An Algorithm for Predicting the
// Intelligibility of Speech Masked by Modulated Noise Maskers", IEEE/ACM
// TASLP 24(11), 2016.

// pystoi's constants: the rate, frame, FFT, bands, lowest band's centre,
// frames per segment and dynamic range.
inline constexpr int kEstoiRate = 10000;
inline constexpr int kEstoiFrame = 256;
inline constexpr int kEstoiHop = kEstoiFrame / 2;
inline constexpr int kEstoiFft = 512;
inline constexpr int kEstoiBands = 15;
inline constexpr double kEstoiLowestCentreHz = 150.0;
inline constexpr int kEstoiSegment = 30;
inline constexpr double kEstoiDynamicRangeDb = 40.0;

// The bins [first, end) of an FFT of kEstoiFft at kEstoiRate that a
// one-third-octave band sums.
struct ThirdOctaveBand {
  int first = 0;
  int end = 0;
};

// pystoi thirdoct(): each band's edges, 2^(+-1/6) about its centre
// 150 x 2^(k/3) Hz, at the nearest bins.
std::array<ThirdOctaveBand, kEstoiBands> ThirdOctaveBands();

// pystoi resample_oct(): `x` from `from_rate` to `to_rate` by
// scipy.signal.resample_poly() with Octave's resample() filter (a Kaiser
// window for 60 dB rejection over a sinc at the lower Nyquist rate, with a
// roll-off a tenth of it). O(n x taps).
std::vector<double> ResampleLikeOctave(std::span<const double> x, int from_rate,
                                       int to_rate);

// pystoi remove_silent_frames(): the Hann-windowed kEstoiFrame frames,
// every kEstoiHop, of both signals whose frame of `x` is within
// kEstoiDynamicRangeDb of its loudest, overlapped and added again. Both
// signals are the same length.
std::pair<std::vector<double>, std::vector<double>> RemoveSilentFrames(
    std::span<const double> x, std::span<const double> y);

// The one-third-octave band envelopes of `x` (pystoi's stft() and its
// product with the band matrix): one row per band, one column per frame.
std::vector<std::vector<double>> BandEnvelopes(std::span<const double> x);

// pystoi row_col_normalize() of two segments (bands x kEstoiSegment
// frames, starting at frame `start` of each envelope), then the sum of
// their products over kEstoiSegment: the segment's intermediate
// intelligibility. pystoi adds noise of 2.2e-16 before each
// normalisation, to avoid dividing by zero; here a row or column with no
// variation is left at zero instead, so the result is deterministic.
double SegmentIntelligibility(const std::vector<std::vector<double>>& x,
                              const std::vector<std::vector<double>>& y,
                              int start);

// ESTOI of `y` against `x`, both 8 kHz and the same length (pystoi
// stoi(x, y, 8000, extended=True)). Fails if fewer than kEstoiSegment
// frames are left after the silent ones are removed (about 0.4 s of
// sound), where pystoi returns 1e-5. O(n).
StatusOr<double> Estoi(std::span<const double> x, std::span<const double> y);

// ESTOI of `prepared`'s output against its original, with the samples of
// the frames outside the mask set to zero in both, so its own removal of
// silent frames drops them. Fails as Estoi() does.
StatusOr<double> MeasureIntelligibility(const PreparedSignals& prepared);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_ESTOI_H_
