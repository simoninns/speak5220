/*
 * File:        frame.cc
 * Module:      lpc
 * Purpose:     A coded TMS5220 frame: silence, stop, repeat, unvoiced or
 *              voiced
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/lpc/frame.h"

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace speak5220::core::lpc {
namespace {

// speak5220 datasheet §8.1: frame lengths in bits.
constexpr int kShortFrameBits = kEnergyBits;
constexpr int kRepeatFrameBits = kEnergyBits + kRepeatBits + kPitchBits;
constexpr int kUnvoicedFrameBits = kRepeatFrameBits + 5 + 5 + 4 + 4;
constexpr int kVoicedFrameBits = kUnvoicedFrameBits + 4 + 4 + 4 + 3 + 3 + 3;
constexpr int kMaxPitchIndex = (1 << kPitchBits) - 1;

std::string Describe(const Frame& frame) {
  std::string text = "energy " + std::to_string(frame.energy) + ", repeat " +
                     std::to_string(frame.repeat ? 1 : 0) + ", pitch " +
                     std::to_string(frame.pitch) + ", K";
  for (const int value : frame.k) {
    text += ' ';
    text += std::to_string(value);
  }
  return text;
}

}  // namespace

FrameType FrameKind(const Frame& frame) {
  if (frame.energy == kSilenceEnergy) {
    return FrameType::kSilence;
  }
  if (frame.energy == kStopEnergy) {
    return FrameType::kStop;
  }
  if (frame.repeat) {
    return FrameType::kRepeat;
  }
  return frame.pitch == 0 ? FrameType::kUnvoiced : FrameType::kVoiced;
}

int FrameBits(const Frame& frame) {
  switch (FrameKind(frame)) {
    case FrameType::kSilence:
    case FrameType::kStop:
      return kShortFrameBits;
    case FrameType::kRepeat:
      return kRepeatFrameBits;
    case FrameType::kUnvoiced:
      return kUnvoicedFrameBits;
    case FrameType::kVoiced:
      return kVoicedFrameBits;
  }
  return kShortFrameBits;
}

std::string_view FrameTypeName(FrameType type) {
  switch (type) {
    case FrameType::kSilence:
      return "silence";
    case FrameType::kStop:
      return "stop";
    case FrameType::kRepeat:
      return "repeat";
    case FrameType::kUnvoiced:
      return "unvoiced";
    case FrameType::kVoiced:
      return "voiced";
  }
  return "silence";
}

int FrameKCount(const Frame& frame) {
  switch (FrameKind(frame)) {
    case FrameType::kUnvoiced:
      return kUnvoicedKCount;
    case FrameType::kVoiced:
      return kVoicedKCount;
    default:
      return 0;
  }
}

Status ValidateFrame(const Frame& frame) {
  if (frame.energy < 0 || frame.energy > kStopEnergy) {
    return InvalidArgumentError("energy index out of range: " +
                                Describe(frame));
  }
  if (frame.pitch < 0 || frame.pitch > kMaxPitchIndex) {
    return InvalidArgumentError("pitch index out of range: " + Describe(frame));
  }
  const FrameType type = FrameKind(frame);
  if ((type == FrameType::kSilence || type == FrameType::kStop) &&
      (frame.repeat || frame.pitch != 0)) {
    return InvalidArgumentError(
        "a silence or stop frame carries only its energy: " + Describe(frame));
  }
  const int carried = FrameKCount(frame);
  for (int i = 0; i < kKCount; ++i) {
    const int value = frame.k.at(static_cast<std::size_t>(i));
    if (i >= carried) {
      if (value != 0) {
        return InvalidArgumentError(
            "K" + std::to_string(i + 1) + " is not carried by a " +
            std::string(FrameTypeName(type)) + " frame: " + Describe(frame));
      }
      continue;
    }
    if (value < 0 || value >= (1 << kKBits.at(static_cast<std::size_t>(i)))) {
      return InvalidArgumentError("K" + std::to_string(i + 1) +
                                  " index out of range: " + Describe(frame));
    }
  }
  return OkStatus();
}

std::vector<Frame> EnsureStop(std::span<const Frame> frames) {
  std::vector<Frame> out;
  for (const Frame& frame : frames) {
    out.push_back(frame);
    if (frame.energy == kStopEnergy) {
      return out;
    }
  }
  out.push_back(Frame::Stop());
  return out;
}

}  // namespace speak5220::core::lpc
