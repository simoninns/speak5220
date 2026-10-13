/*
 * File:        de_esser.h
 * Module:      dsp
 * Purpose:     Turns down the hiss of sibilants (S, SH) in a recording: a
 *              split-band de-esser, which lowers only the treble band, and
 *              only where it is most of the sound
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_DE_ESSER_H_
#define SPEAK5220_CORE_DSP_DE_ESSER_H_

#include <span>
#include <vector>

namespace speak5220::core::dsp {

// How the de-esser works out the treble band's gain.
//
// The band's share of the sound is the band's level over the whole
// recording's level there, each the RMS of a window centred on the sample:
// near 0 dB in a sibilant, whose hiss is nearly all treble, and some 20 dB
// or more below that in a vowel. Where the share is within `depth_db` of
// 0 dB, the band is turned down by the difference, so by the whole depth
// where the band is all of the sound, and less the more of the rest there
// is. The gain then changes by no more than `ramp_db_per_ms`, starting
// before a sibilant (the whole recording is known), so the change is not
// heard as a click.
struct DeEsserShape {
  double window_ms = 10.0;
  double ramp_db_per_ms = 1.0;
};

// The gain the de-esser gives each sample of `band` (the treble band of
// `samples`, sample for sample), 0 to 1: down by up to `depth_db` dB.
// All 1 where `samples` is silent. `band` must be as long as `samples`.
// Pure; O(n).
std::vector<double> DeEsserGain(std::span<const double> samples,
                                std::span<const double> band, int sample_rate,
                                double depth_db,
                                const DeEsserShape& shape = {});

// `samples` with `band` (its treble band) turned down by DeEsserGain():
// samples - (1 - gain) x band. Unchanged where the gain is 1, so with a
// band that is the rest's complement nothing else is touched. Pure; O(n).
std::vector<double> DeEss(std::span<const double> samples,
                          std::span<const double> band, int sample_rate,
                          double depth_db, const DeEsserShape& shape = {});

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_DE_ESSER_H_
