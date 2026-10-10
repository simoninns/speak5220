/*
 * File:        frame.h
 * Module:      lpc
 * Purpose:     A coded TMS5220 frame: silence, stop, repeat, unvoiced or
 *              voiced
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_LPC_FRAME_H_
#define SPEAK5220_CORE_LPC_FRAME_H_

#include <array>
#include <span>
#include <string_view>
#include <vector>

#include "core/lpc/coding_tables.h"
#include "core/util/status.h"

namespace speak5220::core::lpc {

// speak5220 datasheet §8.1: the five kinds of frame.
enum class FrameType { kSilence, kStop, kRepeat, kUnvoiced, kVoiced };

// Energy indices with a special meaning (§8.1).
inline constexpr int kSilenceEnergy = 0;
inline constexpr int kStopEnergy = 15;
// K indices a full frame carries: unvoiced frames carry K1-K4 only.
inline constexpr int kUnvoicedKCount = 4;
inline constexpr int kVoicedKCount = kKCount;
// Field widths in bits (§8.1).
inline constexpr int kEnergyBits = 4;
inline constexpr int kRepeatBits = 1;
inline constexpr int kPitchBits = 6;

// A frame of coded indices. Fields a frame's kind does not carry are zero:
// a silence or stop frame has only its energy, a repeat frame its energy
// and pitch, an unvoiced frame K1-K4. Use the factory functions to build
// one, and ValidateFrame() to check one from outside. A plain value type.
// Re-implements the frame dicts of tms5220enc.py.
struct Frame {
  int energy = 0;
  bool repeat = false;
  int pitch = 0;
  std::array<int, kKCount> k = {};

  static Frame Silence() { return {}; }
  static Frame Stop() { return {.energy = kStopEnergy}; }
  static Frame Repeat(int energy, int pitch) {
    return {.energy = energy, .repeat = true, .pitch = pitch};
  }
  static Frame Unvoiced(int energy,
                        const std::array<int, kUnvoicedKCount>& k4) {
    Frame frame{.energy = energy};
    for (std::size_t i = 0; i < k4.size(); ++i) {
      frame.k.at(i) = k4.at(i);
    }
    return frame;
  }
  static Frame Voiced(int energy, int pitch,
                      const std::array<int, kVoicedKCount>& k10) {
    return {.energy = energy, .pitch = pitch, .k = k10};
  }

  friend bool operator==(const Frame& a, const Frame& b) = default;
};

// The kind of a frame. Re-implements tms5220enc.py frameKind().
FrameType FrameKind(const Frame& frame);

// The length of a frame in bits: 4, 4, 11, 29 or 50 (§8.1).
// Re-implements tms5220enc.py frameBits().
int FrameBits(const Frame& frame);

// "silence", "stop", "repeat", "unvoiced" or "voiced".
std::string_view FrameTypeName(FrameType type);

// The number of K indices a frame of this kind carries (0, 4 or 10).
int FrameKCount(const Frame& frame);

// Checks every field against its width, and that the fields the frame's
// kind does not carry are zero. Re-implements tms5220enc.py _checkFrame().
Status ValidateFrame(const Frame& frame);

// The frames up to and including the first stop frame, with a stop frame
// added if there is none. Re-implements tms5220enc.py _ensureStop().
std::vector<Frame> EnsureStop(std::span<const Frame> frames);

}  // namespace speak5220::core::lpc

#endif  // SPEAK5220_CORE_LPC_FRAME_H_
