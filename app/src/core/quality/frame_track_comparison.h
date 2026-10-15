/*
 * File:        frame_track_comparison.h
 * Module:      quality
 * Purpose:     Compares an encoding's frames with reference frames for the
 *              same speech: voicing, onsets and offsets, energy, filter
 *              shape, pitch, repeats and fade-outs
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_FRAME_TRACK_COMPARISON_H_
#define SPEAK5220_CORE_QUALITY_FRAME_TRACK_COMPARISON_H_

#include <array>
#include <span>
#include <vector>

#include "core/lpc/frame.h"

namespace speak5220::core::quality {

// The frame shifts tried when lining the encoding up with the reference.
inline constexpr int kMaxTrackShift = 3;

// The lags (encoding minus reference, in frames) at which a voicing onset
// or offset of the reference is looked for in the encoding; bin
// lag - kFirstBoundaryLag of a BoundaryLags, and the last bin for none.
inline constexpr int kFirstBoundaryLag = -2;
inline constexpr int kLastBoundaryLag = 3;
inline constexpr int kBoundaryLagBins =
    kLastBoundaryLag - kFirstBoundaryLag + 2;
using BoundaryLags = std::array<int, kBoundaryLagBins>;

// An unvoiced frame of the encoding at this energy index or above where
// the reference starts voicing is a loud hiss.
inline constexpr int kLoudOnsetEnergy = 8;

// The repeat runs counted by length: 1, 2, 3, and 4 or more.
inline constexpr int kRunLengthBins = 4;

// The facts of one frame track by itself.
struct FrameTrackFacts {
  int frames = 0;
  // Voiced, unvoiced and repeat frames.
  int sounding = 0;
  int repeats = 0;
  // Repeat runs by length (1, 2, 3, 4+), and the longest.
  std::array<int, kRunLengthBins> repeat_runs = {};
  int longest_repeat_run = 0;
  // Fade-outs: runs of unvoiced repeat frames of falling energy after an
  // unvoiced frame, ending at silence, the stop or the end (as
  // encoder::AddFades() makes them), and the frames in them.
  int fade_outs = 0;
  int fade_frames = 0;
  int bits = 0;
};

// How an encoding's frames compare with reference frames. Vectors hold
// one value per frame compared, so comparisons can be pooled
// (AddComparison()).
struct FrameTrackComparison {
  // Encoding frame j + shift lines up with reference frame j: the shift
  // within +-kMaxTrackShift with the least mean level difference.
  int shift = 0;
  // Frames sounding in both, and of those, the frames of the same voicing.
  int matched = 0;
  int voicing_agrees = 0;
  // Reference voiced, encoding unvoiced: inside a voiced run, at its first
  // frame, at its last; and of those at the first frame, the frames at
  // kLoudOnsetEnergy or above.
  int voiced_as_unvoiced_inside = 0;
  int voiced_as_unvoiced_onset = 0;
  int voiced_as_unvoiced_offset = 0;
  int loud_onset_frames = 0;
  // Reference unvoiced, encoding voiced.
  int unvoiced_as_voiced = 0;
  // The reference's voicing onsets and offsets, and where the encoding's
  // nearest follow (BoundaryLags).
  int reference_onsets = 0;
  int reference_offsets = 0;
  BoundaryLags onset_lags = {};
  BoundaryLags offset_lags = {};
  // Over frames of the same voicing in both: energy index differences
  // (encoding minus reference), and the RMS in dB of the difference of
  // their filters' log responses less its mean (the filter shape error).
  std::vector<double> voiced_energy_differences;
  std::vector<double> unvoiced_energy_differences;
  std::vector<double> voiced_shape_errors_db;
  std::vector<double> unvoiced_shape_errors_db;
  // Output level difference in dB (encoding minus reference) where both
  // are above 1 code.
  std::vector<double> level_differences_db;
  // Pitch difference in semitones where both are voiced.
  std::vector<double> pitch_differences;
  FrameTrackFacts reference;
  FrameTrackFacts encoded;
};

// The facts of `frames`. Pure. O(frames).
FrameTrackFacts DescribeFrameTrack(std::span<const lpc::Frame> frames);

// Compares `encoded` with `reference`, two frame tracks of the same
// speech, each ending in a stop: both are synthesised
// (synth::ReferenceSynthesizer), lined up by their output levels, and
// compared frame by frame. A repeat frame takes its voicing from its pitch
// and its filter from the last full frame. Pure; thread-safe.
// O(frames x samples per frame).
FrameTrackComparison CompareFrameTracks(std::span<const lpc::Frame> reference,
                                        std::span<const lpc::Frame> encoded);

// Pools `add` into `total`: counts summed, values appended; the shift is
// left as it is.
void AddComparison(FrameTrackComparison& total,
                   const FrameTrackComparison& add);

// The log magnitude response in dB, at kResponseBins frequencies from 0 to
// 4 kHz, of the lattice filter of `k_indices` (coded K1 onwards, `order`
// of them): 20 log10 |1 / A(e^jw)|. O(order x bins).
inline constexpr int kResponseBins = 129;
std::array<double, kResponseBins> FilterResponseDb(
    std::span<const int> k_indices, int order);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_FRAME_TRACK_COMPARISON_H_
