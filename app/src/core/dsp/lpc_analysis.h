/*
 * File:        lpc_analysis.h
 * Module:      dsp
 * Purpose:     Linear-prediction analysis: autocorrelation, Levinson-Durbin,
 *              step-up and prediction error
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_LPC_ANALYSIS_H_
#define SPEAK5220_CORE_DSP_LPC_ANALYSIS_H_

#include <span>
#include <vector>

namespace speak5220::core::dsp {

// The largest reflection-coefficient magnitude Levinson() and Lar() allow.
inline constexpr double kMaxReflection = 0.9999;

// Autocorrelation r[0..order] of `segment`: r[i] = sum s[n] s[n + i].
// Re-implements tms5220enc.py _autocorr(). O(n x order).
std::vector<double> Autocorrelation(std::span<const double> segment, int order);

// Reflection coefficients k1..k_order by Levinson-Durbin from the
// autocorrelation `r` (at least order + 1 values), for the predictor
// e(n) = s(n) + sum a_j s(n - j). Each coefficient is clamped to
// +-kMaxReflection; the recursion stops (leaving zeros) if the error stops
// being positive. Re-implements tms5220enc.py levinson(). O(order^2).
std::vector<double> Levinson(std::span<const double> r, int order);

// The predictor polynomial a[0..p] (a[0] = 1) of reflection coefficients
// k1..kp. Re-implements tms5220enc.py stepUp(). O(p^2).
std::vector<double> StepUp(std::span<const double> k);

// The prediction error a^T R a of polynomial `a` on the Toeplitz matrix of
// autocorrelation `r` (at least a.size() values). Re-implements the matrix
// product of tms5220enc.py _predErr(). O(p^2).
double PredictionError(std::span<const double> a, std::span<const double> r);

// The log-area ratio log((1 - k) / (1 + k)), with k clamped to
// +-kMaxReflection. Re-implements tms5220enc.py _lar().
double LogAreaRatio(double k);

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_LPC_ANALYSIS_H_
