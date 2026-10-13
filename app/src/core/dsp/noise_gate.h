/*
 * File:        noise_gate.h
 * Module:      dsp
 * Purpose:     Turns down the quiet stretches of a recording (breaths, room
 *              noise) relative to its loudest sound, with a soft knee and
 *              gradual opening and closing so word edges survive
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_NOISE_GATE_H_
#define SPEAK5220_CORE_DSP_NOISE_GATE_H_

#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

// A stretch of samples, [begin, end).
struct SampleSpan {
  std::size_t begin = 0;
  std::size_t end = 0;

  friend bool operator==(const SampleSpan& a, const SampleSpan& b) = default;
};

// How the noise gate works out each sample's gain.
//
// The level is the RMS of a window centred on the sample, long enough to
// span a low voice's pitch period so the level does not ripple. Where it is
// at or above the threshold (`below_db` under the loudest level), the gain
// is 1. Below the threshold the gate is a downward expander: the gain falls
// by `expansion_db_per_db` for each dB the level is under it, down to
// `floor_db`, where the sample is made silent. So sound just under the
// threshold is only turned down a little (a soft knee), and only sound well
// under it is muted.
//
// The gain then changes no faster than the gate's speed allows. It closes,
// from open to `floor_db`, over the `close_ms` the user chooses, after the
// sound falls, so a soft word ending fades. It opens over a quarter of that,
// starting before the sound rises (the whole recording is known), so an
// onset is not cut. Both changes are linear in dB, as faders are.
struct NoiseGateShape {
  double window_ms = 20.0;
  double expansion_db_per_db = 3.0;
  double floor_db = -80.0;
  double open_share = 0.25;
};

// The gain the noise gate gives each sample, 0 to 1: closing (from open to
// silent) over `close_ms`, below `below_db` under the recording's loudest
// level. All 1 for samples that are all zero. Pure; O(n).
std::vector<double> NoiseGateGain(std::span<const double> samples,
                                  int sample_rate, double below_db,
                                  double close_ms,
                                  const NoiseGateShape& shape = {});

// `samples` times their NoiseGateGain(). Pure; O(n).
std::vector<double> NoiseGate(std::span<const double> samples, int sample_rate,
                              double below_db, double close_ms,
                              const NoiseGateShape& shape = {});

// How far down the gate must turn a sample for it to count as gated when
// showing what the gate does: 6 dB, half the amplitude.
inline constexpr double kGatedGain = 0.5;

// The stretches the noise gate turns down by more than 6 dB (a gain under
// kGatedGain), in order, for showing what it mutes. Pure; O(n).
std::vector<SampleSpan> GatedSpans(std::span<const double> samples,
                                   int sample_rate, double below_db,
                                   double close_ms,
                                   const NoiseGateShape& shape = {});

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_NOISE_GATE_H_
