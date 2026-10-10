/*
 * File:        frame_codec.h
 * Module:      lpc
 * Purpose:     Packs and parses TMS5220 frames in the chip's bit order
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_LPC_FRAME_CODEC_H_
#define SPEAK5220_CORE_LPC_FRAME_CODEC_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/lpc/frame.h"
#include "core/util/status.h"

namespace speak5220::core::lpc {

// Frames parsed from a bit stream, and the bits they used.
struct ParsedFrames {
  std::vector<Frame> frames;
  std::size_t bits_used = 0;
};

// The chip's bit stream (speak5220 datasheet §8.1, §12.1): each field is
// written most significant bit first, and bytes are filled from bit 0, the
// order in which the chip reads a VSM byte. Stateless; thread-safe.
class FrameCodec {
 public:
  // A limit on the frames Parse() reads, so a stream with no stop code
  // cannot grow without bound.
  static constexpr std::size_t kDefaultMaxFrames = 100000;

  // Packs frames into bytes. Packing ends at the first stop frame; the rest
  // of its byte is zero. Fails if a frame is invalid (ValidateFrame()).
  // Re-implements tms5220enc.py pack(). O(total bits).
  static StatusOr<std::vector<std::uint8_t>> Pack(
      std::span<const Frame> frames);

  // Parses frames up to and including the stop code. If the bits run out
  // mid-frame, the complete frames read so far are returned, and bits_used
  // counts only their bits. Re-implements tms5220enc.py parse().
  static ParsedFrames Parse(std::span<const std::uint8_t> data,
                            std::size_t max_frames = kDefaultMaxFrames);
};

// Reverses the bits of a byte. speak5220 datasheet §6: Read Byte returns a
// VSM byte bit-reversed, and a byte read that way must be reversed again
// before it goes to Speak External. Re-implements tms5220enc.py
// bitReverse().
constexpr std::uint8_t BitReverse(std::uint8_t byte) {
  std::uint8_t out = 0;
  for (int i = 0; i < 8; ++i) {
    out = static_cast<std::uint8_t>((out << 1) | ((byte >> i) & 1U));
  }
  return out;
}

}  // namespace speak5220::core::lpc

#endif  // SPEAK5220_CORE_LPC_FRAME_CODEC_H_
