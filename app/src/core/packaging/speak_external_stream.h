/*
 * File:        speak_external_stream.h
 * Module:      packaging
 * Purpose:     Builds the byte stream a host writes to the FIFO after Speak
 *              External
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PACKAGING_SPEAK_EXTERNAL_STREAM_H_
#define SPEAK5220_CORE_PACKAGING_SPEAK_EXTERNAL_STREAM_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/lpc/frame.h"
#include "core/util/status.h"

namespace speak5220::core::packaging {

// A Speak External stream (speak5220 datasheet §12.9): the packed frames
// in FIFO order, which is the order FrameCodec::Pack() writes, so the
// bytes are sent unreversed. Stateless; thread-safe.
class SpeakExternalStream {
 public:
  // speak5220 datasheet §12.9: a Speak External stream needs at least 9
  // bytes, or speech never starts.
  static constexpr std::size_t kMinimumBytes = 9;

  // Packs the frames up to the first stop (adding one if there is none),
  // with as few silence frames before the stop as make the stream 9 bytes
  // long. Re-implements tms5220enc.py speakExternalStream().
  static StatusOr<std::vector<std::uint8_t>> Build(
      std::span<const lpc::Frame> frames);
};

}  // namespace speak5220::core::packaging

#endif  // SPEAK5220_CORE_PACKAGING_SPEAK_EXTERNAL_STREAM_H_
