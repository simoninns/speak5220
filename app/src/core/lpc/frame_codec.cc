/*
 * File:        frame_codec.cc
 * Module:      lpc
 * Purpose:     Packs and parses TMS5220 frames in the chip's bit order
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/lpc/frame_codec.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

namespace speak5220::core::lpc {
namespace {

constexpr std::size_t kBitsPerByte = 8;

// Appends fields to a byte vector: each field MSB first, bytes filled from
// bit 0 (speak5220 datasheet §12.1).
class BitWriter {
 public:
  void Put(int value, int width) {
    for (int i = width - 1; i >= 0; --i) {
      if (bit_count_ % kBitsPerByte == 0) {
        bytes_.push_back(0);
      }
      if (((value >> i) & 1) != 0) {
        bytes_.back() = static_cast<std::uint8_t>(
            bytes_.back() | (1U << (bit_count_ % kBitsPerByte)));
      }
      ++bit_count_;
    }
  }

  std::vector<std::uint8_t> TakeBytes() { return std::move(bytes_); }

 private:
  std::vector<std::uint8_t> bytes_;
  std::size_t bit_count_ = 0;
};

// Reads fields in the same order. Get() gives nothing once the bits run
// out.
class BitReader {
 public:
  explicit BitReader(std::span<const std::uint8_t> data) : data_(data) {}

  std::optional<int> Get(int width) {
    const auto needed = static_cast<std::size_t>(width);
    if (position_ + needed > data_.size() * kBitsPerByte) {
      return std::nullopt;
    }
    int value = 0;
    for (std::size_t i = 0; i < needed; ++i) {
      const std::uint8_t byte = data_[position_ / kBitsPerByte];
      value = (value << 1) | ((byte >> (position_ % kBitsPerByte)) & 1);
      ++position_;
    }
    return value;
  }

  std::size_t Position() const { return position_; }

 private:
  std::span<const std::uint8_t> data_;
  std::size_t position_ = 0;
};

// Reads one frame, or nothing if the bits run out within it.
std::optional<Frame> ReadFrame(BitReader& reader) {
  const std::optional<int> energy = reader.Get(kEnergyBits);
  if (!energy) {
    return std::nullopt;
  }
  Frame frame{.energy = *energy};
  if (frame.energy == kSilenceEnergy || frame.energy == kStopEnergy) {
    return frame;
  }
  const std::optional<int> repeat = reader.Get(kRepeatBits);
  const std::optional<int> pitch = reader.Get(kPitchBits);
  if (!repeat || !pitch) {
    return std::nullopt;
  }
  frame.repeat = *repeat != 0;
  frame.pitch = *pitch;
  if (frame.repeat) {
    return frame;
  }
  const int count = frame.pitch == 0 ? kUnvoicedKCount : kVoicedKCount;
  for (int i = 0; i < count; ++i) {
    const auto at = static_cast<std::size_t>(i);
    const std::optional<int> value = reader.Get(kKBits.at(at));
    if (!value) {
      return std::nullopt;
    }
    frame.k.at(at) = *value;
  }
  return frame;
}

}  // namespace

StatusOr<std::vector<std::uint8_t>> FrameCodec::Pack(
    std::span<const Frame> frames) {
  BitWriter writer;
  for (const Frame& frame : frames) {
    if (Status status = ValidateFrame(frame); !status.IsOk()) {
      return status;
    }
    writer.Put(frame.energy, kEnergyBits);
    if (frame.energy == kStopEnergy) {
      break;
    }
    if (frame.energy == kSilenceEnergy) {
      continue;
    }
    writer.Put(frame.repeat ? 1 : 0, kRepeatBits);
    writer.Put(frame.pitch, kPitchBits);
    const int count = FrameKCount(frame);
    for (int i = 0; i < count; ++i) {
      const auto at = static_cast<std::size_t>(i);
      writer.Put(frame.k.at(at), kKBits.at(at));
    }
  }
  return writer.TakeBytes();
}

ParsedFrames FrameCodec::Parse(std::span<const std::uint8_t> data,
                               std::size_t max_frames) {
  ParsedFrames parsed;
  BitReader reader(data);
  while (parsed.frames.size() < max_frames) {
    std::optional<Frame> frame = ReadFrame(reader);
    if (!frame) {
      break;
    }
    parsed.frames.push_back(*frame);
    parsed.bits_used = reader.Position();
    if (frame->energy == kStopEnergy) {
      break;
    }
  }
  return parsed;
}

}  // namespace speak5220::core::lpc
