/*
 * File:        acorn_phrom.cpp
 * Module:      packaging
 * Purpose:     Builds and reads 16 KB Acorn PHROM images: header, pointer
 *              table, word names and word data
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/acorn_phrom.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <numeric>
#include <optional>
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
// §12.8: header bytes 0 and 1, as the host reads them.
constexpr std::uint8_t kSignatureFirst = 0x00;
constexpr std::uint8_t kSignatureSecond = 0xFF;
constexpr std::size_t kBitsPerByte = 8;

bool IsAscii(const std::string& text) {
  return std::ranges::all_of(
      text, [](char c) { return static_cast<unsigned char>(c) < kAsciiLimit; });
}

void PutLittleEndian16(std::array<std::uint8_t, kHeaderSize>& header,
                       std::size_t offset, int value) {
  header.at(offset) = static_cast<std::uint8_t>(value & 0xFF);
  header.at(offset + 1) = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

// The little-endian count at `offset` of the header, which is stored
// bit-reversed (§12.8). `image` holds the whole header.
int ReadHeaderCount(std::span<const std::uint8_t> image, std::size_t offset) {
  return lpc::BitReverse(image[offset]) |
         (lpc::BitReverse(image[offset + 1]) << 8);
}

// §12.8: a name character is stored shifted left by one, then
// bit-reversed. Gives nothing for a byte that no printable character
// (32-126) is stored as, which ends a name.
std::optional<char> DecodeNameCharacter(std::uint8_t stored) {
  const std::uint8_t shifted = lpc::BitReverse(stored);
  if ((shifted & 1U) != 0) {
    return std::nullopt;
  }
  const int code = shifted >> 1U;
  if (code < kFirstAscii || code > kLastAscii) {
    return std::nullopt;
  }
  return static_cast<char>(code);
}

}  // namespace

Status ValidateHeaderStrings(std::span<const std::string> strings) {
  std::size_t bytes = 0;
  for (const std::string& line : strings) {
    if (line.empty()) {
      return InvalidArgumentError(
          "a header string cannot be empty: a host stops reading the "
          "strings at an empty one");
    }
    if (!std::ranges::all_of(line, [](char c) {
          return c >= static_cast<char>(kFirstAscii) &&
                 c <= static_cast<char>(kLastAscii);
        })) {
      return InvalidArgumentError(fmt::format(
          "the header string \"{}\" holds a character that is not printable "
          "ASCII",
          line));
    }
    bytes += line.size() + 1;
  }
  if (bytes > static_cast<std::size_t>(kHeaderStringsCapacity)) {
    return InvalidArgumentError(fmt::format(
        "the header strings take {} bytes with a null after each, and a "
        "PHROM header holds {}",
        bytes, kHeaderStringsCapacity));
  }
  return OkStatus();
}

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
  if (Status status = ValidateHeaderStrings(options.strings); !status.IsOk()) {
    return status;
  }
  std::string text;
  for (const std::string& line : options.strings) {
    text += line;
    text += '\0';
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
  // A lone stop code after the last word, for ASCII entries with no word.
  // When every entry names a word, as in Acorn's PHROM A, there is none,
  // and the words may fill the bank to its last byte (§12.8).
  if (std::cmp_less(options.ascii_map.size(), kAsciiCount)) {
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
      if (!options.ascii_map.contains(c)) {
        pointers[c] = static_cast<int>(position);
      }
    }
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

StatusOr<std::vector<ReadWord>> AcornPhrom::Read(
    std::span<const std::uint8_t> image) {
  if (image.size() != kPhromSize) {
    return InvalidArgumentError("an Acorn PHROM image is 16384 bytes, not " +
                                std::to_string(image.size()));
  }
  // §12.8: bytes 0 and 1 of the header are 0x00, 0xFF.
  if (lpc::BitReverse(image[0]) != kSignatureFirst ||
      lpc::BitReverse(image[1]) != kSignatureSecond) {
    return InvalidArgumentError(
        "not an Acorn PHROM image: its header does not start with 0x00, "
        "0xFF");
  }
  const int ascii_count = ReadHeaderCount(image, kAsciiCountOffset);
  const int pointer_count = ReadHeaderCount(image, kPointerCountOffset);
  if (ascii_count > pointer_count) {
    return DataLossError(
        "the PHROM header counts do not fit: " + std::to_string(ascii_count) +
        " ASCII entries but " + std::to_string(pointer_count) + " pointers");
  }
  // §12.8: word number N's pointer is at byte 2N, from N = 32, and one
  // byte, 0xFF, follows the last pointer; data can start after it.
  const std::size_t table_end = 2 * (static_cast<std::size_t>(kFirstAscii) +
                                     static_cast<std::size_t>(pointer_count));
  if (table_end >= kPhromSize) {
    return DataLossError("the PHROM pointer table (" +
                         std::to_string(pointer_count) +
                         " pointers) does not fit in 16 KB");
  }
  const std::size_t data_start = table_end + 1;
  const int first_number = kFirstAscii + ascii_count;
  const int word_count = pointer_count - ascii_count;

  std::vector<ReadWord> words(static_cast<std::size_t>(word_count));
  for (int i = 0; i < word_count; ++i) {
    const int number = first_number + i;
    const std::size_t at = 2 * static_cast<std::size_t>(number);
    // §12.7: Read and Branch replaces the low 14 bits of the address, so a
    // pointer cannot leave its bank; the bits above are not used.
    const int offset = (image[at] | (image[at + 1] << 8)) & kPointerMask;
    if (std::cmp_less(offset, data_start)) {
      return DataLossError("PHROM word " + std::to_string(number) +
                           " points into the header or the pointer table "
                           "(offset " +
                           std::to_string(offset) + ")");
    }
    ReadWord& word = words[static_cast<std::size_t>(i)];
    word.number = number;
    word.offset = offset;
  }

  // The words in address order, so each name can be bounded by the end of
  // the word before it.
  std::vector<std::size_t> order(words.size());
  std::iota(order.begin(), order.end(), std::size_t{0});
  std::ranges::stable_sort(order, [&words](std::size_t a, std::size_t b) {
    return words[a].offset < words[b].offset;
  });
  std::size_t previous_end = data_start;
  const ReadWord* previous = nullptr;
  for (const std::size_t index : order) {
    ReadWord& word = words[index];
    const auto offset = static_cast<std::size_t>(word.offset);
    lpc::ParsedFrames parsed = lpc::FrameCodec::Parse(image.subspan(offset));
    if (parsed.frames.empty() ||
        parsed.frames.back().energy != lpc::kStopEnergy) {
      return DataLossError("PHROM word " + std::to_string(word.number) +
                           " (offset " + std::to_string(offset) +
                           ") has no stop code within the bank");
    }
    word.frames = std::move(parsed.frames);
    if (previous != nullptr && previous->offset == word.offset) {
      // Two pointers to the same data: the same word, with the same name.
      word.name = previous->name;
    } else {
      // §12.8: the name is stored backwards, its first character next to
      // the data, between the end of the previous word and this one.
      for (std::size_t at = offset; at > previous_end; --at) {
        const std::optional<char> character =
            DecodeNameCharacter(image[at - 1]);
        if (!character) {
          break;
        }
        word.name += *character;
      }
    }
    const std::size_t end =
        offset + (parsed.bits_used + kBitsPerByte - 1) / kBitsPerByte;
    previous_end = std::max(previous_end, end);
    previous = &word;
  }
  return words;
}

StatusOr<ReadPhrom> AcornPhrom::ReadImage(std::span<const std::uint8_t> image) {
  StatusOr<std::vector<ReadWord>> words = Read(image);
  if (!words.IsOk()) {
    return words.GetStatus();
  }
  ReadPhrom result;
  // §12.8: the header's strings, bit-reversed, each ending in a null.
  std::string text;
  constexpr auto kStringsEnd =
      kStringsStart + static_cast<std::size_t>(kHeaderStringsCapacity);
  for (std::size_t i = kStringsStart; i < kStringsEnd; ++i) {
    const auto character = static_cast<char>(lpc::BitReverse(image[i]));
    if (character != '\0') {
      if (character < ' ' || character > '~') {
        break;
      }
      text += character;
      continue;
    }
    if (text.empty()) {
      break;
    }
    result.strings.push_back(std::move(text));
    text.clear();
  }
  // Read() checked that the ASCII entries lie within the pointer table.
  std::map<int, int> number_at;  // data offset -> lowest word number
  for (const ReadWord& word : *words) {
    number_at.emplace(word.offset, word.number);
  }
  const int ascii_count = ReadHeaderCount(image, kAsciiCountOffset);
  for (int code = kFirstAscii;
       code < kFirstAscii + ascii_count && code <= kLastAscii; ++code) {
    const std::size_t at = 2 * static_cast<std::size_t>(code);
    const int pointer = (image[at] | (image[at + 1] << 8)) & kPointerMask;
    if (const auto found = number_at.find(pointer); found != number_at.end()) {
      result.ascii_words[code] = found->second;
    }
  }
  result.words = *std::move(words);
  return result;
}

}  // namespace speak5220::core::packaging
