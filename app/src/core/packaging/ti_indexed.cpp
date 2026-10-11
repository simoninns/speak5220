/*
 * File:        ti_indexed.cpp
 * Module:      packaging
 * Purpose:     Builds a 16 KB image in TI's indexed format
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/ti_indexed.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"

namespace speak5220::core::packaging {
namespace {

// §12.8: pointers keep 14 bits.
constexpr int kPointerHighMask = 0x3F;

}  // namespace

StatusOr<PhromImage> TiIndexed::Build(std::span<const Word> words) {
  const std::size_t count = words.size();
  if (count > static_cast<std::size_t>(kMaxWords)) {
    return InvalidArgumentError("at most 255 words, not " +
                                std::to_string(count));
  }
  std::vector<std::uint8_t> image(kPhromSize, 0);
  image[0] = 0;
  image[1] = static_cast<std::uint8_t>(count);
  std::size_t position = 2 * (count + 1);
  PhromImage result;
  for (std::size_t i = 0; i < count; ++i) {
    const Word& word = words[i];
    const std::vector<lpc::Frame> frames = lpc::EnsureStop(word.frames);
    StatusOr<std::vector<std::uint8_t>> data = lpc::FrameCodec::Pack(frames);
    if (!data.IsOk()) {
      return data.GetStatus();
    }
    if (position + data->size() + 1 > kPhromSize) {
      return ResourceExhaustedError("words do not fit in 16 KB (at \"" +
                                    word.name + "\")");
    }
    std::ranges::copy(*data, std::span(image).subspan(position).begin());
    image[2 * (i + 1)] = static_cast<std::uint8_t>(position & 0xFF);
    image[2 * (i + 1) + 1] =
        static_cast<std::uint8_t>((position >> 8) & kPointerHighMask);
    result.table.push_back({.number = static_cast<int>(i + 1),
                            .name = word.name,
                            .offset = static_cast<int>(position),
                            .bytes = static_cast<int>(data->size()),
                            .frames = static_cast<int>(frames.size())});
    position += data->size() + 1;
  }
  result.bytes = std::move(image);
  return result;
}

}  // namespace speak5220::core::packaging
