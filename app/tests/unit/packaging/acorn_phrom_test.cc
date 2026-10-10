/*
 * File:        acorn_phrom_test.cc
 * Module:      packaging tests
 * Purpose:     Unit tests of the Acorn PHROM layout: header, pointers,
 *              names, ASCII entries, the null word and overflow
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/acorn_phrom.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::packaging {
namespace {

using lpc::BitReverse;
using lpc::Frame;
using lpc::FrameCodec;
using speak5220::testing::SpeechLikeFrames;

std::vector<Word> ThreeWords() {
  std::mt19937 rng(7);
  return {{"HELLO", SpeechLikeFrames(rng, 10)},
          {"A", SpeechLikeFrames(rng, 5)},
          {"WORLD", SpeechLikeFrames(rng, 20)}};
}

int Pointer(const std::vector<std::uint8_t>& image, int number) {
  const std::size_t at = 2 * static_cast<std::size_t>(number);
  return image[at] | (image[at + 1] << 8);
}

// Re-implements test_tms5220enc.py Packaging.testAcornPhrom.
class AcornPhromTest : public ::testing::Test {
 protected:
  void SetUp() override {
    words_ = ThreeWords();
    AcornPhromOptions options;
    options.ascii_map = {{'H', "HELLO"}, {65, "A"}, {'W', "WORLD"}};
    StatusOr<PhromImage> built = AcornPhrom::Build(words_, options);
    ASSERT_TRUE(built.IsOk()) << built.GetStatus().ToString();
    image_ = std::move(built).Value();
  }

  std::vector<Word> words_;
  PhromImage image_;
};

TEST_F(AcornPhromTest, HeaderIsBitReversed) {
  ASSERT_EQ(image_.bytes.size(), kPhromSize);
  // As Read Byte returns it.
  std::vector<std::uint8_t> host;
  host.reserve(64);
  for (std::size_t i = 0; i < 64; ++i) {
    host.push_back(BitReverse(image_.bytes[i]));
  }
  EXPECT_EQ(host[0], 0x00);
  EXPECT_EQ(host[1], 0xFF);
  const std::string strings(host.begin() + 2, host.begin() + 56);
  EXPECT_EQ(strings.substr(0, 37),
            std::string("(C)2026 speak5220\0Speak5220 PHROM\0"
                        "1.00",
                        37));
  EXPECT_EQ(host[56] | (host[57] << 8), 95);
  EXPECT_EQ(host[58] | (host[59] << 8), 95 + 3);
  EXPECT_EQ(std::vector<std::uint8_t>(host.begin() + 60, host.end()),
            std::vector<std::uint8_t>({0, 2, 0, 2}));
}

TEST_F(AcornPhromTest, PointerTableEndsWithFF) {
  const int last = 32 + 95 + 3 - 1;
  EXPECT_EQ(image_.bytes[static_cast<std::size_t>(2 * last + 2)], 0xFF);
  // The first word's name follows the pointer table's 0xFF.
  EXPECT_EQ(image_.table[0].offset - static_cast<int>(words_[0].name.size()),
            2 * last + 3);
}

TEST_F(AcornPhromTest, WordsParseBackAtTheirPointers) {
  for (std::size_t i = 0; i < words_.size(); ++i) {
    const int number = AcornPhrom::kFirstWordNumber + static_cast<int>(i);
    const int offset = Pointer(image_.bytes, number);
    const WordTableEntry& entry = image_.table[i];
    EXPECT_EQ(entry.number, number);
    EXPECT_EQ(entry.offset, offset);
    EXPECT_EQ(entry.name, words_[i].name);
    const lpc::ParsedFrames parsed =
        FrameCodec::Parse(std::span<const std::uint8_t>(image_.bytes)
                              .subspan(static_cast<std::size_t>(offset)));
    EXPECT_EQ(parsed.frames, words_[i].frames);
    EXPECT_EQ(entry.bytes, static_cast<int>((parsed.bits_used + 7) / 8));
    EXPECT_EQ(entry.frames, static_cast<int>(words_[i].frames.size()));
    // Pointers reach only their own bank.
    EXPECT_LT(offset, 0x4000);
  }
}

TEST_F(AcornPhromTest, NamesAreStoredBackwardsShiftedAndReversed) {
  for (std::size_t i = 0; i < words_.size(); ++i) {
    const auto offset = static_cast<std::size_t>(image_.table[i].offset);
    const std::string& name = words_[i].name;
    for (std::size_t j = 0; j < name.size(); ++j) {
      EXPECT_EQ(BitReverse(image_.bytes[offset - 1 - j]) >> 1, name[j]);
    }
  }
}

TEST_F(AcornPhromTest, AsciiEntriesPointToTheirWords) {
  EXPECT_EQ(Pointer(image_.bytes, 'H'), Pointer(image_.bytes, 127));
  EXPECT_EQ(Pointer(image_.bytes, 65), Pointer(image_.bytes, 128));
  EXPECT_EQ(Pointer(image_.bytes, 'W'), Pointer(image_.bytes, 129));
}

TEST_F(AcornPhromTest, UnmappedAsciiEntriesSpeakNothing) {
  const auto offset = static_cast<std::size_t>(Pointer(image_.bytes, 'Z'));
  const lpc::ParsedFrames parsed = FrameCodec::Parse(
      std::span<const std::uint8_t>(image_.bytes).subspan(offset));
  EXPECT_EQ(parsed.frames, std::vector<Frame>({Frame::Stop()}));
}

TEST_F(AcornPhromTest, WordsDoNotOverlap) {
  for (std::size_t i = 0; i + 1 < image_.table.size(); ++i) {
    const WordTableEntry& a = image_.table[i];
    const WordTableEntry& b = image_.table[i + 1];
    EXPECT_LE(a.offset + a.bytes,
              b.offset - static_cast<int>(words_[i + 1].name.size()));
  }
}

// Re-implements test_tms5220enc.py Packaging.testAcornTooBig.
TEST(AcornPhromOverflowTest, RejectsWordsThatDoNotFit) {
  std::mt19937 rng(10);
  const std::vector<Frame> big = SpeechLikeFrames(rng, 400);
  std::vector<Word> words;
  words.reserve(20);
  for (int i = 0; i < 20; ++i) {
    words.push_back({"W" + std::to_string(i), big});
  }
  const StatusOr<PhromImage> built = AcornPhrom::Build(words, {});
  ASSERT_FALSE(built.IsOk());
  EXPECT_EQ(built.GetStatus().Code(), StatusCode::kResourceExhausted);
}

TEST(AcornPhromOptionsTest, RejectsAsciiEntryNamingNoWord) {
  const std::vector<Word> words = {{"ONE", {Frame::Stop()}}};
  AcornPhromOptions options;
  options.ascii_map = {{'1', "TWO"}};
  EXPECT_EQ(AcornPhrom::Build(words, options).GetStatus().Code(),
            StatusCode::kInvalidArgument);
  options.ascii_map = {{127, "ONE"}};
  EXPECT_EQ(AcornPhrom::Build(words, options).GetStatus().Code(),
            StatusCode::kInvalidArgument);
}

TEST(AcornPhromOptionsTest, RejectsLongHeaderStrings) {
  AcornPhromOptions options;
  options.strings = {std::string(54, 'x')};
  EXPECT_EQ(AcornPhrom::Build({}, options).GetStatus().Code(),
            StatusCode::kInvalidArgument);
}

TEST(AcornPhromOptionsTest, AddsStopToOpenWord) {
  const std::vector<Word> words = {{"OPEN", {Frame::Repeat(3, 4)}}};
  const StatusOr<PhromImage> built = AcornPhrom::Build(words, {});
  ASSERT_TRUE(built.IsOk());
  EXPECT_EQ(built->table[0].frames, 2);
}

}  // namespace
}  // namespace speak5220::core::packaging
