/*
 * File:        encoder_types.h
 * Module:      encoder
 * Purpose:     The values passed between the encoder's stages: the pitch
 *              track, frame features, the plan of frame kinds and K
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_ENCODER_TYPES_H_
#define SPEAK5220_CORE_ENCODER_ENCODER_TYPES_H_

#include <array>
#include <vector>

namespace speak5220::core::encoder {

// The encoder works on 8 kHz audio only.
inline constexpr int kEncoderSampleRate = 8000;
// Samples per frame: 25 ms (speak5220 datasheet §8.5).
inline constexpr int kFrameSamples = 200;
// The autocorrelation lags each frame keeps: order 10, plus r[0].
inline constexpr int kLpcOrder = 10;
inline constexpr int kUnvoicedLpcOrder = 4;
inline constexpr int kAutocorrelationSize = kLpcOrder + 1;
// Output sample (counted from the first frame read) at the middle of frame
// j's own stretch, for j = 0: its values are half-way reached by the end of
// IP 4 of its glide (sample 125), reached in IP 0 (201-225), and half-way
// to the next frame's by sample 325 (§8.5, §8.7, §12.2).
inline constexpr int kReachedSample = 225;

// The pitch tracker's result: the voicing and period at each hop.
struct PitchTrack {
  // Hop centres, in input samples (every 5 ms from sample 0).
  std::vector<int> hops;
  // The period in samples at each hop, 0 where unvoiced.
  std::vector<double> periods;
};

// A frame's voicing decision.
struct FrameVoicing {
  bool voiced = false;
  // The period in samples; 0 when unvoiced.
  double period = 0.0;
};

// One frame's analysis, before quantisation. Re-implements the feature
// dicts of tms5220enc.py analyse().
struct FrameFeatures {
  // The frame's centre, in input samples.
  int centre = 0;
  // Hann-weighted RMS over 25 ms around the centre.
  double level = 0.0;
  // The same after LoudnessHighPass(): the level the energies aim at.
  double loudness = 0.0;
  bool voiced = false;
  double period = 0.0;
  // Lag-windowed autocorrelation of the pre-emphasised (voiced) and plain
  // (unvoiced) signal.
  std::array<double, kAutocorrelationSize> rv = {};
  std::array<double, kAutocorrelationSize> ru = {};
  // At or below the silence gate.
  bool gated = false;
};

// The kind a frame is planned as, before its energy is set.
enum class PlanKind { kSilence, kVoiced, kUnvoiced, kRepeat };

// One frame of the plan: its kind, its K indices (10 voiced, 4 unvoiced;
// a repeat carries the K of its segment's full frame) and its pitch index.
struct PlanEntry {
  PlanKind kind = PlanKind::kSilence;
  std::vector<int> k;
  int pitch = 0;

  friend bool operator==(const PlanEntry& a, const PlanEntry& b) = default;
};

// What level setting changed.
struct LevelStats {
  // Sounding frames made silent: too loud at energy 1, or by the limiter.
  int silenced = 0;
  // Frames whose energy the limiter lowered.
  int limited = 0;
  // Frames that were sounding in the plan.
  int sounding = 0;
};

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_ENCODER_TYPES_H_
