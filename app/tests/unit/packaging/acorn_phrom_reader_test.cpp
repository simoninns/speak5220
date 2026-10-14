/*
 * File:        acorn_phrom_reader_test.cpp
 * Module:      packaging tests
 * Purpose:     Unit tests of reading Acorn PHROM images back: round trips
 *              of names and frames, missing names, and corrupt images
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/packaging/acorn_phrom.h"
#include "core/packaging/phrom_image.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::packaging {
namespace {

using lpc::BitReverse;
using lpc::Frame;
using speak5220::testing::SpeechLikeFrames;

// Byte offsets of the header counts and the pointer of word `number`
// (speak5220 datasheet §12.8).
constexpr std::size_t kAsciiCountOffset = 56;
constexpr std::size_t kPointerCountOffset = 58;

std::size_t PointerAt(int number) {
  return 2 * static_cast<std::size_t>(number);
}

void SetPointer(std::vector<std::uint8_t>& image, int number, int value) {
  image[PointerAt(number)] = static_cast<std::uint8_t>(value & 0xFF);
  image[PointerAt(number) + 1] = static_cast<std::uint8_t>(value >> 8);
}

// Writes a header count as Build() does: little-endian, bit-reversed.
void SetHeaderCount(std::vector<std::uint8_t>& image, std::size_t offset,
                    int value) {
  image[offset] = BitReverse(static_cast<std::uint8_t>(value & 0xFF));
  image[offset + 1] = BitReverse(static_cast<std::uint8_t>(value >> 8));
}

std::vector<Word> SampleWords() {
  std::mt19937 rng(11);
  return {{"HELLO", SpeechLikeFrames(rng, 10)},
          {"A", SpeechLikeFrames(rng, 5)},
          {"(TONE1)", SpeechLikeFrames(rng, 20)},
          {"TWO WORDS", SpeechLikeFrames(rng, 8)}};
}

PhromImage BuildImage(const std::vector<Word>& words) {
  // ASCII entries for the words that have a one-character name.
  AcornPhromOptions options;
  for (const Word& word : words) {
    if (word.name.size() == 1) {
      options.ascii_map[word.name[0]] = word.name;
    }
  }
  StatusOr<PhromImage> built = AcornPhrom::Build(words, options);
  EXPECT_TRUE(built.IsOk()) << built.GetStatus().ToString();
  return built.IsOk() ? *std::move(built) : PhromImage{};
}

TEST(AcornPhromReadTest, RoundTripsNamesAndFrames) {
  const std::vector<Word> words = SampleWords();
  const PhromImage image = BuildImage(words);
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  ASSERT_EQ(read->size(), words.size());
  for (std::size_t i = 0; i < words.size(); ++i) {
    const ReadWord& word = (*read)[i];
    EXPECT_EQ(word.number, AcornPhrom::kFirstWordNumber + static_cast<int>(i));
    EXPECT_EQ(word.name, words[i].name);
    EXPECT_EQ(word.offset, image.table[i].offset);
    EXPECT_EQ(word.frames, words[i].frames);
  }
}

TEST(AcornPhromReadTest, RoundTripsAFullImage) {
  std::mt19937 rng(12);
  constexpr int kWords = 150;
  std::vector<Word> words;
  words.reserve(kWords);
  for (int i = 0; i < kWords; ++i) {
    words.push_back({"WORD " + std::to_string(i), SpeechLikeFrames(rng, 20)});
  }
  const PhromImage image = BuildImage(words);
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  ASSERT_EQ(read->size(), words.size());
  for (std::size_t i = 0; i < words.size(); ++i) {
    EXPECT_EQ((*read)[i].name, words[i].name);
    EXPECT_EQ((*read)[i].frames, words[i].frames);
  }
}

TEST(AcornPhromReadTest, AddsTheStopBuildAdded) {
  const std::vector<Word> words = {{"OPEN", {Frame::Repeat(3, 4)}}};
  const PhromImage image = BuildImage(words);
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  ASSERT_EQ(read->size(), 1U);
  EXPECT_EQ((*read)[0].frames,
            std::vector<Frame>({Frame::Repeat(3, 4), Frame::Stop()}));
}

TEST(AcornPhromReadTest, ReadsAnImageWithNoWords) {
  const StatusOr<PhromImage> built = AcornPhrom::Build({}, {});
  ASSERT_TRUE(built.IsOk());
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(built->bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  EXPECT_TRUE(read->empty());
}

TEST(AcornPhromReadTest, ReadsAWordWithNoNameAsUnnamed) {
  std::vector<Word> words = SampleWords();
  words[1].name.clear();
  const PhromImage image = BuildImage({words[0], words[1], words[2]});
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  ASSERT_EQ(read->size(), 3U);
  EXPECT_EQ((*read)[0].name, "HELLO");
  EXPECT_EQ((*read)[1].name, "");
  EXPECT_EQ((*read)[1].frames, words[1].frames);
  EXPECT_EQ((*read)[2].name, "(TONE1)");
}

TEST(AcornPhromReadTest, NameEndsAtAByteNoCharacterIsStoredAs) {
  const std::vector<Word> words = SampleWords();
  PhromImage image = BuildImage(words);
  // The far end of "(TONE1)", its last character, becomes padding.
  const auto offset = static_cast<std::size_t>(image.table[2].offset);
  image.bytes[offset - words[2].name.size()] = 0x00;
  // And every byte of "TWO WORDS" becomes padding.
  const auto last = static_cast<std::size_t>(image.table[3].offset);
  for (std::size_t i = 1; i <= words[3].name.size(); ++i) {
    image.bytes[last - i] = 0xFF;
  }
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  EXPECT_EQ((*read)[2].name, "(TONE1");
  EXPECT_EQ((*read)[3].name, "");
}

TEST(AcornPhromReadTest, PointersToTheSameDataShareTheName) {
  const std::vector<Word> words = SampleWords();
  PhromImage image = BuildImage(words);
  SetPointer(image.bytes, AcornPhrom::kFirstWordNumber + 2,
             image.table[0].offset);
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  EXPECT_EQ((*read)[2].name, "HELLO");
  EXPECT_EQ((*read)[2].frames, words[0].frames);
  // "A" now follows HELLO's data directly, as before.
  EXPECT_EQ((*read)[1].name, "A");
}

TEST(AcornPhromReadTest, RejectsAWrongSize) {
  const PhromImage image = BuildImage(SampleWords());
  const std::span<const std::uint8_t> bytes(image.bytes);
  EXPECT_EQ(AcornPhrom::Read(bytes.first(kPhromSize - 1)).GetStatus().Code(),
            StatusCode::kInvalidArgument);
  EXPECT_EQ(AcornPhrom::Read({}).GetStatus().Code(),
            StatusCode::kInvalidArgument);
  std::vector<std::uint8_t> doubled = image.bytes;
  doubled.insert(doubled.end(), image.bytes.begin(), image.bytes.end());
  EXPECT_EQ(AcornPhrom::Read(doubled).GetStatus().Code(),
            StatusCode::kInvalidArgument);
}

TEST(AcornPhromReadTest, RejectsAnImageWithAnotherHeader) {
  const std::vector<std::uint8_t> zeros(kPhromSize, 0);
  EXPECT_EQ(AcornPhrom::Read(zeros).GetStatus().Code(),
            StatusCode::kInvalidArgument);
}

TEST(AcornPhromReadTest, RejectsCountsThatDoNotFit) {
  const PhromImage image = BuildImage(SampleWords());
  std::vector<std::uint8_t> bytes = image.bytes;
  SetHeaderCount(bytes, kAsciiCountOffset, 200);
  EXPECT_EQ(AcornPhrom::Read(bytes).GetStatus().Code(), StatusCode::kDataLoss);
  bytes = image.bytes;
  SetHeaderCount(bytes, kPointerCountOffset, 0xFFFF);
  EXPECT_EQ(AcornPhrom::Read(bytes).GetStatus().Code(), StatusCode::kDataLoss);
  bytes = image.bytes;
  SetHeaderCount(bytes, kPointerCountOffset, 8160);  // table ends at 16384
  EXPECT_EQ(AcornPhrom::Read(bytes).GetStatus().Code(), StatusCode::kDataLoss);
}

TEST(AcornPhromReadTest, RejectsAPointerIntoTheHeaderOrTable) {
  const PhromImage image = BuildImage(SampleWords());
  const int last = AcornPhrom::kFirstWordNumber + 3;
  for (const int target : {0, 63, 64, 2 * last + 2}) {
    std::vector<std::uint8_t> bytes = image.bytes;
    SetPointer(bytes, AcornPhrom::kFirstWordNumber + 1, target);
    const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(bytes);
    ASSERT_FALSE(read.IsOk()) << target;
    EXPECT_EQ(read.GetStatus().Code(), StatusCode::kDataLoss);
    EXPECT_NE(read.GetStatus().Message().find("word 128"), std::string::npos)
        << read.GetStatus().Message();
  }
}

TEST(AcornPhromReadTest, IgnoresPointerBitsAboveTheBank) {
  const std::vector<Word> words = SampleWords();
  PhromImage image = BuildImage(words);
  // §12.7: Read and Branch keeps only the low 14 bits.
  const std::size_t at = PointerAt(AcornPhrom::kFirstWordNumber);
  image.bytes[at + 1] = static_cast<std::uint8_t>(image.bytes[at + 1] | 0xC0);
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  EXPECT_EQ((*read)[0].frames, words[0].frames);
}

TEST(AcornPhromReadTest, RejectsAWordWithNoStopInTheBank) {
  const PhromImage image = BuildImage(SampleWords());
  std::vector<std::uint8_t> bytes = image.bytes;
  // Silence frames (energy 0) to the end of the bank.
  const auto offset = static_cast<std::size_t>(image.table.back().offset);
  std::fill(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end(),
            std::uint8_t{0});
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(bytes);
  ASSERT_FALSE(read.IsOk());
  EXPECT_EQ(read.GetStatus().Code(), StatusCode::kDataLoss);
  EXPECT_NE(read.GetStatus().Message().find("no stop code"), std::string::npos);
}

// Whatever the bytes, Read() returns words ending in a stop or an error.
void ExpectWellFormedOrError(const std::vector<std::uint8_t>& bytes) {
  const StatusOr<std::vector<ReadWord>> read = AcornPhrom::Read(bytes);
  if (!read.IsOk()) {
    EXPECT_NE(read.GetStatus().Code(), StatusCode::kOk);
    return;
  }
  for (const ReadWord& word : *read) {
    ASSERT_FALSE(word.frames.empty());
    EXPECT_EQ(word.frames.back(), Frame::Stop());
    EXPECT_GE(word.offset, 0);
    EXPECT_LT(word.offset, static_cast<int>(kPhromSize));
  }
}

TEST(AcornPhromReadTest, SurvivesGarbage) {
  std::mt19937 rng(13);
  std::uniform_int_distribution<int> byte(0, 255);
  for (int round = 0; round < 50; ++round) {
    std::vector<std::uint8_t> bytes(kPhromSize);
    for (std::uint8_t& b : bytes) {
      b = static_cast<std::uint8_t>(byte(rng));
    }
    // A valid signature, so the reader goes on past the header check.
    bytes[0] = 0x00;
    bytes[1] = 0xFF;
    ExpectWellFormedOrError(bytes);
  }
}

TEST(AcornPhromReadTest, SurvivesCorruptedImages) {
  const PhromImage image = BuildImage(SampleWords());
  std::mt19937 rng(14);
  std::uniform_int_distribution<std::size_t> position(0, kPhromSize - 1);
  std::uniform_int_distribution<int> byte(0, 255);
  for (int round = 0; round < 200; ++round) {
    std::vector<std::uint8_t> bytes = image.bytes;
    for (int i = 0; i < 8; ++i) {
      bytes[position(rng)] = static_cast<std::uint8_t>(byte(rng));
    }
    ExpectWellFormedOrError(bytes);
  }
}

TEST(AcornPhromReadTest, ReadImageGivesTheHeaderStringsAndAsciiEntries) {
  const std::vector<Word> words = SampleWords();
  AcornPhromOptions options;
  options.strings = {"(C)1982 Acorn", "Word PHROM A", "1.00"};
  // Two entries for the word A (number 128), one for HELLO (127).
  options.ascii_map = {{'A', "A"}, {'a', "A"}, {'H', "HELLO"}};
  const StatusOr<PhromImage> image = AcornPhrom::Build(words, options);
  ASSERT_TRUE(image.IsOk()) << image.GetStatus().ToString();
  const StatusOr<ReadPhrom> read = AcornPhrom::ReadImage(image->bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  EXPECT_EQ(read->strings, options.strings);
  EXPECT_EQ(read->ascii_words,
            (std::map<int, int>{{'A', 128}, {'H', 127}, {'a', 128}}));
  EXPECT_EQ(read->words.size(), words.size());
}

TEST(AcornPhromReadTest, EntriesPointingAtTheNullWordAreLeftOut) {
  const PhromImage image = BuildImage(SampleWords());
  const StatusOr<ReadPhrom> read = AcornPhrom::ReadImage(image.bytes);
  ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
  // BuildImage() maps only "A"; the other 94 point at the null word.
  EXPECT_EQ(read->ascii_words, (std::map<int, int>{{'A', 128}}));
}

}  // namespace
}  // namespace speak5220::core::packaging
