/*
 * File:        frame_statistics.h
 * Module:      lpc
 * Purpose:     Counts a phrase's frames by kind, its bits and bytes, and
 *              its bit rate
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_LPC_FRAME_STATISTICS_H_
#define SPEAK5220_CORE_LPC_FRAME_STATISTICS_H_

#include <span>

#include "core/lpc/frame.h"

namespace speak5220::core::lpc {

// speak5220 datasheet §8.1: each frame lasts 25 ms (200 samples at 8 kHz).
inline constexpr double kFrameSeconds = 0.025;

// What a phrase's frames add up to (plan §5, the encoder panel). A plain
// value type.
struct FrameStatistics {
  // Frames before the stop, and how many of them are of each kind.
  int frames = 0;
  int voiced = 0;
  int unvoiced = 0;
  int repeat = 0;
  int silence = 0;
  // The bits of every frame up to and including the stop (one is counted
  // if there is none), and the bytes they pack to.
  int bits = 0;
  int bytes = 0;

  // The frames' length in seconds.
  double Seconds() const { return frames * kFrameSeconds; }
  // Bits per second of speech; 0 with no frames.
  double BitRate() const;
};

// The statistics of `frames`, up to the first stop. Frames are not
// validated. Pure; O(frames).
FrameStatistics SummariseFrames(std::span<const Frame> frames);

}  // namespace speak5220::core::lpc

#endif  // SPEAK5220_CORE_LPC_FRAME_STATISTICS_H_
