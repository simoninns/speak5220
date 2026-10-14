/*
 * File:        pitch_tracker.h
 * Module:      encoder
 * Purpose:     Voicing and pitch period every 5 ms: YIN candidates joined by
 *              a Viterbi search, held near an F0 estimator's track
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_PITCH_TRACKER_H_
#define SPEAK5220_CORE_ENCODER_PITCH_TRACKER_H_

#include <cstddef>
#include <span>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/quality/pitch_estimator.h"

namespace speak5220::core::encoder {

// Tracks voicing and pitch through an utterance. Implementations are
// stateless between calls; see each for thread safety.
class IPitchTracker {
 public:
  virtual ~IPitchTracker() = default;

  // The voicing and period at each 5 ms hop of `x` (8 kHz, high-passed).
  // Uses options.voicing_threshold and options.voicing_evidence.
  virtual PitchTrack Track(std::span<const double> x,
                           const EncoderOptions& options) = 0;
};

// The hop and the lags of the YIN analysis.
inline constexpr int kPitchHop = 40;
inline constexpr int kYinMaxLag = 165;

// YIN's cumulative mean normalised difference function (CMNDF) at each
// hop.
struct YinAnalysis {
  // Hop centres: 0, 40, 80, ... up to the end of the input.
  std::vector<int> centres;
  // One row of kYinMaxLag + 1 values per hop; row[0] is 1.
  std::vector<std::vector<double>> cmndf;
};

// A local minimum of a CMNDF row, refined by a parabola.
struct PitchCandidate {
  double lag = 0.0;
  double value = 0.0;
};

// The voicing evidence besides periodicity, per hop.
struct VoicingEvidence {
  // A tilt score from r1/r0 of the signal pre-emphasised by 0.99: -2, -1,
  // 1, 2 or 4 from flat to steeply low-pass.
  std::vector<double> tilt;
  // Zero crossings per 10 ms.
  std::vector<double> crossings;
};

// The CMNDF (de Cheveigne and Kawahara's YIN) over a 30 ms window at each
// hop, by FFT correlation. Re-implements tms5220enc.py _yin().
// O(hops x 1024 log 1024).
YinAnalysis Yin(dsp::IFft& fft, std::span<const double> x);

// Up to five local minima of a CMNDF row between lags 15 and 161, lowest
// value first, each refined by a parabola through its neighbours.
// Re-implements tms5220enc.py _candidates().
std::vector<PitchCandidate> PitchCandidates(std::span<const double> cmndf);

// The tilt score and zero-crossing rate over a centred 30 ms Hamming
// window at each of `centres`. Re-implements tms5220enc.py
// _tiltAndCrossings().
VoicingEvidence MeasureVoicingEvidence(std::span<const double> x,
                                       std::span<const int> centres);

// The period at each of `hops` hops from F0 values (Hz, 0 where
// unvoiced) every quality::kPitchHop samples, value k at sample
// k * quality::kPitchHop: an even hop takes its value's period, an odd hop
// the period of the geometric mean of the values either side, or of the
// one of them that is voiced; 0 where neither is. Pure. O(hops).
std::vector<double> AnchorPeriods(std::span<const double> f0, std::size_t hops);

// IPitchTracker by YIN candidates and a Viterbi path. The costs: the
// CMNDF value of a candidate; an unvoiced cost at the voicing threshold,
// which rises for loud low-pass hops and falls for fricatives, weighted by
// the tilt and zero crossings; a log-period jump cost between hops (an
// octave jump costs a lot); a voicing switch cost; and a prior on the
// period. Where an anchor is given and finds the hop voiced, the prior
// holds the period near the anchor's (AnchorPeriods() of its track of the
// band-limited signal, quality::BandLimit()); elsewhere it is a mild one
// towards the utterance's median period. The anchor (RAPT in the
// application) keeps the path off a harmonic that the 1 kHz low-passed
// signal repeats at more strongly than at the period, such as the 4th of
// a vowel whose first formant boosts it; it never decides the voicing.
// Without an anchor, re-implements tms5220enc.py trackPitch(); the anchor
// replaces its utterance prior where it finds voicing. Thread-safe if the
// IFft and the anchor are.
class YinPitchTracker : public IPitchTracker {
 public:
  // `fft` must outlive the tracker.
  explicit YinPitchTracker(dsp::IFft& fft);
  // `fft` and `anchor` must outlive the tracker.
  YinPitchTracker(dsp::IFft& fft, quality::IPitchEstimator& anchor);

  PitchTrack Track(std::span<const double> x,
                   const EncoderOptions& options) override;

 private:
  dsp::IFft* fft_;
  quality::IPitchEstimator* anchor_ = nullptr;
};

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_PITCH_TRACKER_H_
