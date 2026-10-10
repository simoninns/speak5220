/*
 * File:        speak_external_stream.cc
 * Module:      packaging
 * Purpose:     Builds the byte stream a host writes to the FIFO after Speak
 *              External
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/speak_external_stream.h"

#include <cstdint>
#include <span>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"

namespace speak5220::core::packaging {

StatusOr<std::vector<std::uint8_t>> SpeakExternalStream::Build(
    std::span<const lpc::Frame> frames) {
  std::vector<lpc::Frame> stream = lpc::EnsureStop(frames);
  // Each silence frame adds 4 bits, so at most 18 are ever needed.
  while (true) {
    StatusOr<std::vector<std::uint8_t>> data = lpc::FrameCodec::Pack(stream);
    if (!data.IsOk() || data->size() >= kMinimumBytes) {
      return data;
    }
    stream.insert(stream.end() - 1, lpc::Frame::Silence());
  }
}

}  // namespace speak5220::core::packaging
