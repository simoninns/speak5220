/*
 * File:        normaliser.h
 * Module:      dsp
 * Purpose:     Brings a recording's speech to a set loudness and evens it
 *              out, as broadcast processing does: speech-gated loudness, a
 *              soft-knee compressor, and a lookahead peak limiter
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_NORMALISER_H_
#define SPEAK5220_CORE_DSP_NORMALISER_H_

#include <optional>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

// How the normaliser measures loudness and shapes the gain.
//
// Loudness is measured as ITU-R BS.1770 gates it, but over blocks short
// enough for a phrase and without its K-weighting (defined for 48 kHz, and
// little different over speech's band): the mean power of `block_ms`
// blocks, every `hop_ms`, leaving out blocks below `absolute_gate_db` and
// then those more than `relative_gate_db` below the mean of the rest, so
// pauses and breaths do not pull it down. Levels are in dB relative to an
// RMS of 1 (full scale).
//
// The normaliser first scales the recording to the target loudness. A
// compressor then evens it out: each sample's level is the RMS of a
// `window_ms` window centred on it, and above a threshold
// `threshold_below_db` under the target, so over nearly all of the speech,
// the level rises by only 1 / ratio dB for each dB, with a soft knee
// `knee_db` wide around the threshold so the change is gradual.
// The compressor's gain falls by up to `attack_db_per_ms` before a loud
// stretch (the whole recording is known) and recovers by up to
// `release_db_per_ms` after it. The recording is then scaled to the target
// loudness again (make-up gain), and a peak limiter keeps every sample
// within `ceiling_db` of full scale, turning down by up to
// `limiter_attack_db_per_ms` before a peak and recovering by up to
// `limiter_release_db_per_ms` after it. So the loudness is the target
// unless the limiter has had to take some away.
struct NormaliserShape {
  double block_ms = 100.0;
  double hop_ms = 25.0;
  double absolute_gate_db = -70.0;
  double relative_gate_db = -10.0;
  double window_ms = 20.0;
  double threshold_below_db = 10.0;
  double knee_db = 6.0;
  double attack_db_per_ms = 1.0;
  double release_db_per_ms = 0.1;
  double ceiling_db = -1.0;
  double limiter_attack_db_per_ms = 2.0;
  double limiter_release_db_per_ms = 0.5;
};

// The speech loudness of `samples`, in dB relative to full scale, gated as
// NormaliserShape describes; none when no block passes the gates (silence).
// A recording shorter than a block is one block. Pure; O(n).
std::optional<double> SpeechLoudness(std::span<const double> samples,
                                     int sample_rate,
                                     const NormaliserShape& shape = {});

// The gain the normaliser gives each sample: bringing the speech loudness
// to `target_db` (relative to full scale) and compressing what is louder
// by `ratio` (1: no compression), with every sample's peak kept within the
// ceiling. All 1 for silence. Pure; O(n).
std::vector<double> NormaliserGain(std::span<const double> samples,
                                   int sample_rate, double target_db,
                                   double ratio,
                                   const NormaliserShape& shape = {});

// `samples` times their NormaliserGain(). Pure; O(n).
std::vector<double> Normalise(std::span<const double> samples, int sample_rate,
                              double target_db, double ratio,
                              const NormaliserShape& shape = {});

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_NORMALISER_H_
