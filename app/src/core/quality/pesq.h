/*
 * File:        pesq.h
 * Module:      quality
 * Purpose:     ITU-T P.862 narrowband PESQ at 8 kHz, mapped to MOS-LQO by
 *              ITU-T P.862.1
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_PESQ_H_
#define SPEAK5220_CORE_QUALITY_PESQ_H_

#include <span>

#include "core/util/status.h"

namespace speak5220::core::quality {

// A PESQ result.
struct PesqScore {
  // ITU-T P.862 §10.2.16: the raw score, -0.5 to 4.5.
  float raw = 0.0F;
  // ITU-T P.862.1: the raw score mapped to MOS-LQO, 1.0 to 4.5. The score
  // the application shows (plan §3.3).
  float mos_lqo = 0.0F;
};

// ITU-T P.862.1: the mapping from a raw P.862 score to MOS-LQO.
float MosLqoFromRaw(float raw);

// Shortest signal PESQ accepts, in samples: a quarter of a second.
inline constexpr int kPesqMinSamples = 2000;

// Scores `degraded` against `reference`, both at 8 kHz in any consistent
// scale, with P.862 narrowband (the IRS receive filter). Both signals are
// first divided by their joint peak and rounded to single precision, as
// the Python `pesq` package does, so the scores match the reference data.
// Fails for a signal shorter than kPesqMinSamples, a silent or non-finite
// one, or when PESQ finds no speech to align. O(n log n) in the signal
// length; about 8 ms per second of speech. Thread-safe.
StatusOr<PesqScore> ScorePesq(std::span<const double> reference,
                              std::span<const double> degraded);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_PESQ_H_
