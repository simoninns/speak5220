/*
 * File:        envelope_distortion.h
 * Module:      quality
 * Purpose:     The spectral envelope measure: log spectral distortion
 *              between the LPC envelopes of an original and the chip's
 *              output
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_ENVELOPE_DISTORTION_H_
#define SPEAK5220_CORE_QUALITY_ENVELOPE_DISTORTION_H_

#include <span>
#include <vector>

#include "core/quality/lpc_envelope.h"
#include "core/quality/signal_preparation.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// The envelope analysis (plan §3.3): order-10 LPC, as the chip's, of
// Hamming-windowed 25 ms frames (the chip's), compared over 100 to 3500 Hz
// on a 512-point grid.
inline constexpr int kEnvelopeOrder = 10;
inline constexpr int kEnvelopeFrame = 200;
inline constexpr int kEnvelopeFft = 512;
inline constexpr double kEnvelopeLowHz = 100.0;
inline constexpr double kEnvelopeHighHz = 3500.0;

// The classic thresholds of transparent LPC quantisation: a frame over
// 2 dB is an outlier, and one over 4 dB a large one.
inline constexpr double kOutlierDb = 2.0;
inline constexpr double kLargeOutlierDb = 4.0;

// The spectral envelope measure of an encoding.
struct EnvelopeDistortion {
  // The mean log spectral distortion, in dB, over the frames counted.
  double mean_db = 0.0;
  // The percentages of those frames over kOutlierDb and kLargeOutlierDb.
  double over_2db = 0.0;
  double over_4db = 0.0;
  // The frames counted.
  int frames = 0;
};

// The RMS difference, in dB, between two envelopes (in dB, on the same
// grid) over bins first..last inclusive.
double LogSpectralDistortion(std::span<const double> a_db,
                             std::span<const double> b_db, int first, int last);

// The log spectral distortion between the envelope shapes (1/|A|, the gain
// left out) of `prepared`'s original and output, frame by frame, over the
// kEnvelopeFrame frames whose centre the mask counts as speech. A frame
// whose original is silent is not counted; one whose output is silent is
// compared with a flat shape. With no frame counted, all values are 0.
// Fails if the analysis does otherwise. O(n). Thread-safe if `lpc` is.
StatusOr<EnvelopeDistortion> MeasureEnvelopeDistortion(
    const PreparedSignals& prepared, ILpcAnalysis& lpc);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_ENVELOPE_DISTORTION_H_
