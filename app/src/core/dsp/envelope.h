/*
 * File:        envelope.h
 * Module:      dsp
 * Purpose:     The level and gain envelopes the preprocessing's dynamics
 *              (noise gate, de-esser, normaliser) share: a centred RMS
 *              level, and gains whose changes are spread into ramps
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_ENVELOPE_H_
#define SPEAK5220_CORE_DSP_ENVELOPE_H_

#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

// `ms` in samples at `sample_rate`, rounded, at least 1.
std::size_t SamplesOf(double ms, int sample_rate);

// Each sample's level: the RMS of the `window_ms` window centred on it,
// shortened at the ends. Pure; O(n), from a running sum of squares.
std::vector<double> CentredRms(std::span<const double> samples, int sample_rate,
                               double window_ms);

// Spreads each dip in `gain_db` into ramps, so the gain rises by no more
// than `after_step` dB a sample after a dip and `before_step` dB a sample
// before one: each value becomes the lowest of the ramps from the dips.
// No value rises. O(n).
void SpreadDips(std::vector<double>& gain_db, double after_step,
                double before_step);

// Spreads each peak in `gain_db` into ramps, so the gain falls by no more
// than `after_step` dB a sample after a peak and `before_step` dB a sample
// before one: each value becomes the highest of the ramps from the peaks.
// No value falls. O(n).
void SpreadPeaks(std::vector<double>& gain_db, double after_step,
                 double before_step);

// 10^(gain_db / 20) for each value: dB to an amplitude gain. Pure; O(n).
std::vector<double> DbToGain(std::span<const double> gain_db);

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_ENVELOPE_H_
