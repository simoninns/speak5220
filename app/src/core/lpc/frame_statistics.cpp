/*
 * File:        frame_statistics.cpp
 * Module:      lpc
 * Purpose:     Counts a phrase's frames by kind, its bits and bytes, and
 *              its bit rate
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/lpc/frame_statistics.h"

#include <span>

#include "core/lpc/frame.h"

namespace speak5220::core::lpc {

double FrameStatistics::BitRate() const {
  return frames == 0 ? 0.0 : bits / Seconds();
}

FrameStatistics SummariseFrames(std::span<const Frame> frames) {
  constexpr int kBitsPerByte = 8;
  FrameStatistics stats;
  bool stopped = false;
  for (const Frame& frame : frames) {
    stats.bits += FrameBits(frame);
    switch (FrameKind(frame)) {
      case FrameType::kStop:
        stopped = true;
        break;
      case FrameType::kVoiced:
        ++stats.voiced;
        break;
      case FrameType::kUnvoiced:
        ++stats.unvoiced;
        break;
      case FrameType::kRepeat:
        ++stats.repeat;
        break;
      case FrameType::kSilence:
        ++stats.silence;
        break;
    }
    if (stopped) {
      break;
    }
    ++stats.frames;
  }
  if (!stopped) {
    stats.bits += FrameBits(Frame::Stop());
  }
  stats.bytes = (stats.bits + kBitsPerByte - 1) / kBitsPerByte;
  return stats;
}

}  // namespace speak5220::core::lpc
