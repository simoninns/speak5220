/*
 * File:        voicing_classifier.h
 * Module:      encoder
 * Purpose:     Each frame's voicing and period from the pitch track, with
 *              onsets voiced from the hops the chip plays and octave
 *              errors corrected against its neighbours
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

// IVoicingClassifier: CentredVoicing(), then VoiceOnsets(), then
// CorrectOctaves(). Stateless; thread-safe.
class VoicingClassifier : public IVoicingClassifier {
 public:
  std::vector<FrameVoicing> Classify(const PitchTrack& track,
                                     std::span<const int> centres) override;
};

// Each frame voiced when most of the three hops nearest its centre are,
// with their median period; hops beyond the track are not counted.
// Re-implements the vote of tms5220enc.py analyse().
std::vector<FrameVoicing> CentredVoicing(const PitchTrack& track,
                                         std::span<const int> centres);

// The chip plays a frame's voicing over the 25 ms from its centre to the
// next frame's (speak5220 datasheet §8.6 and §8.7), so a vowel that
// starts just after a centre would be heard a frame late, as a burst of
// noise at the vowel's level. An unvoiced frame of `voicing` whose next
// frame is voiced therefore becomes voiced when most of the five hops
// from its centre on are (hops 0 to +4, the time it is heard for), with
// the median period of those voiced hops. Decided on `voicing` as given,
// so a frame voiced here does not voice the one before it. Not in the
// reference encoder: on Acorn's PHROM it brings the encoder's onsets
// nearer the PHROM's own (HLD §9). O(n).
void VoiceOnsets(const PitchTrack& track, std::span<const int> centres,
                 std::span<FrameVoicing> voicing);

// A voiced frame whose period is about twice or half the median of its
// voiced neighbours (two frames either side) is moved an octave, in frame
// order. Re-implements the octave fix of tms5220enc.py analyse().
void CorrectOctaves(std::span<FrameVoicing> voicing);

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
