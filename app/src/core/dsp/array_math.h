/*
 * File:        array_math.h
 * Module:      dsp
 * Purpose:     Sums, means, quantiles and windows with NumPy's order of
 *              operations, so the encoder can match the Python reference
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_ARRAY_MATH_H_
#define SPEAK5220_CORE_DSP_ARRAY_MATH_H_

#include <span>
#include <vector>

namespace speak5220::core::dsp {

// The sum of `values` by NumPy's pairwise summation (numpy.sum on a
// contiguous float64 array): eight running sums over blocks of up to 128
// values, halved recursively above that. Rounds the same way as NumPy,
// which a plain loop does not. O(n).
double PairwiseSum(std::span<const double> values);

// numpy.mean: PairwiseSum() divided by the count; 0 for no values.
double Mean(std::span<const double> values);

// The dot product, summed in order. O(n).
double Dot(std::span<const double> a, std::span<const double> b);

// numpy.median: the middle value, or the mean of the two middle values.
// 0 for no values. O(n log n).
double Median(std::span<const double> values);

// numpy.percentile with its default "linear" method; `percent` is 0-100.
// 0 for no values. O(n log n).
double Percentile(std::span<const double> values, double percent);

// numpy.hanning(m): the symmetric Hann window, zero at both ends.
std::vector<double> HannWindow(int m);

// numpy.hamming(m): the symmetric Hamming window.
std::vector<double> HammingWindow(int m);

// numpy.hanning(m + 2)[1:-1]: a Hann window of m values without its zero
// end points, as the encoder's level measures use.
std::vector<double> InnerHannWindow(int m);

// `values` rounded to float, as the audio interfaces take them.
std::vector<float> ToFloat(std::span<const double> values);

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_ARRAY_MATH_H_
