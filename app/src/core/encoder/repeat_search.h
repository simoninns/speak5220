/*
 * File:        repeat_search.h
 * Module:      encoder
 * Purpose:     Quantises each frame's K and chooses silence, full frames
 *              and repeats by dynamic programming over bits and distortion
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_REPEAT_SEARCH_H_
#define SPEAK5220_CORE_ENCODER_REPEAT_SEARCH_H_

#include <span>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"

namespace speak5220::core::encoder {

// Bits of a full voiced frame, a full unvoiced frame and a repeat (§8.1).
inline constexpr int kVoicedFrameBits = 50;
inline constexpr int kUnvoicedFrameBits = 29;
inline constexpr int kRepeatFrameBits = 11;
// The longest run of a full frame and its repeats.
inline constexpr int kMaxRepeatRun = 8;

// Plans the frame kinds, K and pitch before level setting.
// Implementations are stateless.
class IRepeatSearch {
 public:
  virtual ~IRepeatSearch() = default;

  // One plan entry per frame of `features`, without trailing silence.
  // Uses options.repeats and options.repeat_lambda.
  virtual std::vector<PlanEntry> Plan(std::span<const FrameFeatures> features,
                                      const EncoderOptions& options) = 0;
};

// IRepeatSearch as tms5220enc.py _kindsAndK(): gated frames are silence;
// the rest have their K quantised (order 10 voiced, 4 unvoiced) and their
// pitch coded. Within each run of sounding frames of one voicing, repeats
// are chosen by dynamic programming over segments (a full frame and up to
// kMaxRepeatRun - 1 repeats) on bits + repeat_lambda x distortion, where a
// frame's distortion is how much its prediction error rises, in dB, under
// the segment's K instead of its own. A segment's K is its first frame's,
// or those quantised from its pooled normalised autocorrelation, whichever
// costs less. O(frames x kMaxRepeatRun^2 x quantisation). Stateless;
// thread-safe.
class RepeatSearch : public IRepeatSearch {
 public:
  std::vector<PlanEntry> Plan(std::span<const FrameFeatures> features,
                              const EncoderOptions& options) override;
};

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_REPEAT_SEARCH_H_
