/*
 * File:        quantiser.h
 * Module:      encoder
 * Purpose:     Quantises reflection coefficients, pitch periods and energies
 *              to the TMS5220's coding tables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_QUANTISER_H_
#define SPEAK5220_CORE_ENCODER_QUANTISER_H_

#include <span>
#include <vector>

namespace speak5220::core::encoder {

// Coded K indices and the prediction error they give.
struct QuantisedK {
  std::vector<int> indices;
  double error = 0.0;
};

// The decoded values (table value / 512) of K indices k1..kn.
std::vector<double> KValues(std::span<const int> indices);

// The prediction error a^T R a of the filter of K indices `indices`, on
// autocorrelation `r` (at least indices.size() + 1 values). Re-implements
// tms5220enc.py _predErr(). O(n^2).
double QuantisedPredictionError(std::span<const int> indices,
                                std::span<const double> r);

// K indices for reflection coefficients `k`: the nearest table entries in
// log-area ratio, then up to three passes of a coordinate search (each
// index moved by 1 or 2) that lowers the prediction error on `r` by more
// than a relative 1e-9 (speak5220 datasheet §12.6). Re-implements
// tms5220enc.py quantiseK(). O(passes x n^3).
QuantisedK QuantiseK(std::span<const double> k, std::span<const double> r);

// The pitch index (1-63) whose period (table value + 1, §8.6) is nearest
// to `period` samples in log terms (§12.4). Re-implements tms5220enc.py
// pitchIndex().
int PitchIndex(double period);

// The energy index (1-14) whose table value is nearest to `value` in dB;
// 1 for a value of 0 or less. Re-implements tms5220enc.py _energyIndex().
int EnergyIndex(double value);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_QUANTISER_H_
