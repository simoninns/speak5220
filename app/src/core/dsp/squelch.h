/*
 * File:        squelch.h
 * Module:      dsp
 * Purpose:     Mutes the quiet stretches of a recording (breaths, room
 *              noise) relative to its loudest sound, with a hold and soft
 *              edges so word endings survive
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_SQUELCH_H_
#define SPEAK5220_CORE_DSP_SQUELCH_H_

#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

// The timing of the squelch, in milliseconds.
struct SquelchTiming {
  // The level is the RMS of blocks this long, this far apart.
  double block_ms = 10.0;
  double hop_ms = 2.5;
  // Sound is kept this long after the level falls below the threshold,
  // so a soft word ending is not cut...
  double hold_ms = 60.0;
  // ...and from this long before it rises above it, so an onset is not.
  double lead_ms = 10.0;
  // The gain moves between 0 and 1 over this long at each edge, so the
  // cut makes no click.
  double ramp_ms = 5.0;
};

// A stretch of samples, [begin, end).
struct SampleSpan {
  std::size_t begin = 0;
  std::size_t end = 0;

  friend bool operator==(const SampleSpan& a, const SampleSpan& b) = default;
};

// The gain the squelch gives each sample, 0 to 1. A block is open when its
// RMS is more than `below_db` under the loudest block's; open blocks are
// widened by the hold and the lead, and the gain ramps between open (1)
// and closed (0) outside them. All 1 for samples that are all zero. Pure;
// O(n).
std::vector<double> SquelchGain(std::span<const double> samples,
                                int sample_rate, double below_db,
                                const SquelchTiming& timing = {});

// `samples` times their SquelchGain(): every stretch quieter than
// `below_db` under the loudest block made silent. Pure; O(n).
std::vector<double> Squelch(std::span<const double> samples, int sample_rate,
                            double below_db, const SquelchTiming& timing = {});

// The stretches the squelch lowers (a gain under 1), in order, for showing
// what it mutes. Pure; O(n).
std::vector<SampleSpan> SquelchedSpans(std::span<const double> samples,
                                       int sample_rate, double below_db,
                                       const SquelchTiming& timing = {});

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_SQUELCH_H_
