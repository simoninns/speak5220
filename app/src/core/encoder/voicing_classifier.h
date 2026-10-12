/*
 * File:        voicing_classifier.h
 * Module:      encoder
 * Purpose:     Each frame's voicing and period from the pitch track, with
 *              octave errors corrected against its neighbours
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_VOICING_CLASSIFIER_H_
#define SPEAK5220_CORE_ENCODER_VOICING_CLASSIFIER_H_

#include <span>
#include <vector>

#include "core/encoder/encoder_types.h"

namespace speak5220::core::encoder {

// Decides each frame's voicing and period. Implementations are stateless.
class IVoicingClassifier {
 public:
  virtual ~IVoicingClassifier() = default;

  // One decision per frame centre (in input samples), from `track`.
  virtual std::vector<FrameVoicing> Classify(const PitchTrack& track,
                                             std::span<const int> centres) = 0;
};

// IVoicingClassifier by majority over the three hops nearest each frame's
// centre: voiced when most of them are, with their median period. Then a
// voiced frame whose period is about twice or half the median of its
// voiced neighbours (two frames either side) is moved an octave, in frame
// order. Re-implements the voicing parts of tms5220enc.py analyse().
// Stateless; thread-safe.
class VoicingClassifier : public IVoicingClassifier {
 public:
  std::vector<FrameVoicing> Classify(const PitchTrack& track,
                                     std::span<const int> centres) override;
};

// Removes short islands of voicing: a run of frames shorter than
// `min_run` with runs of the other kind on both sides takes their kind.
// Unvoiced islands go first, each given the mean of its neighbours'
// periods; then voiced islands become unvoiced. Runs at either end are
// kept, and `min_run` 1 or less changes nothing. Not in the reference
// encoder: on Acorn's PHROM it brings the encoder's voicing nearer the
// PHROM's own (HLD §9). O(n).
void SmoothVoicingRuns(std::span<FrameVoicing> voicing, int min_run);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_VOICING_CLASSIFIER_H_
