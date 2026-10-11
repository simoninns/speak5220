/*
 * File:        acorn_phrom.cpp
 * Module:      packaging
 * Purpose:     Builds a 16 KB Acorn PHROM image: header, pointer table,
 *              word names and word data
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/acorn_phrom.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"

namespace speak5220::core::packaging {
namespace {

// speak5220 datasheet §12.8, the Acorn PHROM header (64 bytes).
constexpr std::size_t kHeaderSize = 64;
constexpr std::size_t kStringsStart = 2;
constexpr std::size_t kStringsCapacity = 54;  // bytes 2-55, nulls included
constexpr std::size_t kAsciiCountOffset = 56;
constexpr std::size_t kPointerCountOffset = 58;
constexpr std::size_t kTrailerOffset = 60;
// Bytes 60-63 as in the Acorn PHROM A; their use is not known, so a new
// image copies them (§12.8).
constexpr std::array<std::uint8_t, 4> kTrailer = {0x00, 0x02, 0x00, 0x02};
// The "ASCII associated" entries: one per printable character.
constexpr int kFirstAscii = 32;
constexpr int kLastAscii = 126;
constexpr int kAsciiCount = kLastAscii - kFirstAscii + 1;  // 95
// §12.8: pointers are 14-bit offsets within the bank.
constexpr int kPointerMask = 0x3FFF;
constexpr std::uint8_t kTableEnd = 0xFF;
constexpr int kAsciiLimit = 0x80;

bool IsAscii(const std::string& text) {
  return std::ranges::all_of(
      text, [](char c) { return static_cast<unsigned char>(c) < kAsciiLimit; });
}

void PutLittleEndian16(std::array<std::uint8_t, kHeaderSize>& header,
                       std::size_t offset, int value) {
  header.at(offset) = static_cast<std::uint8_t>(value & 0xFF);
  header.at(offset + 1) = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

}  // namespace

StatusOr<PhromImage> AcornPhrom::Build(std::span<const Word> words,
                                       const AcornPhromOptions& options) {
  std::vector<std::uint8_t> image(kPhromSize, 0);
  const int word_count = static_cast<int>(words.size());
  const int pointer_count = kAsciiCount + word_count;
  const int last_number = kFirstAscii + pointer_count - 1;
  // The first byte after the last pointer.
  const std::size_t table_end = 2 * static_cast<std::size_t>(last_number) + 2;
  if (table_end + 1 > kPhromSize) {
    return ResourceExhaustedError("too many words for one PHROM: " +
                                  std::to_string(word_count));
  }

  // The header, stored bit-reversed (§12.8).
  std::array<std::uint8_t, kHeaderSize> header = {};
  header[0] = 0x00;
  header[1] = 0xFF;
  std::string text;
  for (const std::string& line : options.strings) {
    if (!IsAscii(line)) {
      return InvalidArgumentError("header string is not ASCII: " + line);
    }
    text += line;
    text += '\0';
  }
  if (text.size() > kStringsCapacity) {
    return InvalidArgumentError(
        "header strings too long (54 bytes with their nulls)");
  }
  std::ranges::copy(text, std::span(header).subspan(kStringsStart).begin());
  PutLittleEndian16(header, kAsciiCountOffset, kAsciiCount);
  PutLittleEndian16(header, kPointerCountOffset, pointer_count);
  std::ranges::copy(kTrailer,
                    std::span(header).subspan(kTrailerOffset).begin());
  for (std::size_t i = 0; i < kHeaderSize; ++i) {
    image[i] = lpc::BitReverse(header.at(i));
  }
  image[table_end] = kTableEnd;

  PhromImage result;
  std::map<int, int> pointers;         // word number -> offset
  std::map<std::string, int> numbers;  // name -> word number (last wins)
  std::size_t position = table_end + 1;
  for (int i = 0; i < word_count; ++i) {
    const Word& word = words[static_cast<std::size_t>(i)];
    if (!IsAscii(word.name)) {
      return InvalidArgumentError("word name is not ASCII: " + word.name);
    }
    const std::vector<lpc::Frame> frames = lpc::EnsureStop(word.frames);
    StatusOr<std::vector<std::uint8_t>> data = lpc::FrameCodec::Pack(frames);
    if (!data.IsOk()) {
      return data.GetStatus();
    }
    const std::string& name = word.name;
    if (position + name.size() + data->size() > kPhromSize) {
      return ResourceExhaustedError("words do not fit in 16 KB (at \"" + name +
                                    "\")");
    }
    // §12.8: the name backwards, its first character next to the data,
    // each character shifted left by one and bit-reversed.
    for (std::size_t j = 0; j < name.size(); ++j) {
      const auto character = static_cast<std::uint8_t>(name[j]);
      image[position + name.size() - 1 - j] =
          lpc::BitReverse(static_cast<std::uint8_t>(character << 1));
    }
    position += name.size();
    const std::size_t offset = position;
    std::ranges::copy(*data, std::span(image).subspan(offset).begin());
    position += data->size();
    const int number = kFirstWordNumber + i;
    pointers[number] = static_cast<int>(offset);
    numbers[name] = number;
    result.table.push_back({.number = number,
                            .name = name,
                            .offset = static_cast<int>(offset),
                            .bytes = static_cast<int>(data->size()),
                            .frames = static_cast<int>(frames.size())});
  }

  // A lone stop code after the last word, for ASCII entries with no word.
  if (position + 1 > kPhromSize) {
    return ResourceExhaustedError("words do not fit in 16 KB");
  }
  const std::array<lpc::Frame, 1> stop = {lpc::Frame::Stop()};
  StatusOr<std::vector<std::uint8_t>> stop_byte = lpc::FrameCodec::Pack(stop);
  if (!stop_byte.IsOk()) {
    return stop_byte.GetStatus();
  }
  image[position] = stop_byte->front();
  for (int c = kFirstAscii; c <= kLastAscii; ++c) {
    pointers[c] = static_cast<int>(position);
  }
  for (const auto& [code, name] : options.ascii_map) {
    if (code < kFirstAscii || code > kLastAscii) {
      return InvalidArgumentError("ASCII entry out of range: " +
                                  std::to_string(code));
    }
    const auto found = numbers.find(name);
    if (found == numbers.end()) {
      return InvalidArgumentError("ASCII entry " + std::to_string(code) +
                                  " names no word: " + name);
    }
    pointers[code] = pointers[found->second];
  }
  for (int number = kFirstAscii; number <= last_number; ++number) {
    const int value = pointers[number] & kPointerMask;
    const std::size_t at = 2 * static_cast<std::size_t>(number);
    image[at] = static_cast<std::uint8_t>(value & 0xFF);
    image[at + 1] = static_cast<std::uint8_t>(value >> 8);
  }
  result.bytes = std::move(image);
  return result;
}

}  // namespace speak5220::core::packaging
