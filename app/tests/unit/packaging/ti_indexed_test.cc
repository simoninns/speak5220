/*
 * File:        ti_indexed_test.cc
 * Module:      packaging tests
 * Purpose:     Unit tests of TI's indexed layout: count, pointers, spare
 *              bytes and overflow
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/ti_indexed.h"

#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::packaging {
namespace {

using lpc::FrameCodec;
using speak5220::testing::SpeechLikeFrames;

int Pointer(const std::vector<std::uint8_t>& image, std::size_t word) {
  return image[2 * word] | (image[2 * word + 1] << 8);
}

// Re-implements test_tms5220enc.py Packaging.testTiIndexed.
TEST(TiIndexedTest, CountAndPointersLocateEachWord) {
  std::mt19937 rng(7);
  const std::vector<Word> words = {{"HELLO", SpeechLikeFrames(rng, 10)},
                                   {"A", SpeechLikeFrames(rng, 5)},
                                   {"WORLD", SpeechLikeFrames(rng, 20)}};
  const StatusOr<PhromImage> built = TiIndexed::Build(words);
  ASSERT_TRUE(built.IsOk()) << built.GetStatus().ToString();
  const std::vector<std::uint8_t>& image = built->bytes;
  ASSERT_EQ(image.size(), kPhromSize);
  EXPECT_EQ(image[0], 0);
  EXPECT_EQ(image[1], words.size());
  for (std::size_t i = 0; i < words.size(); ++i) {
    const int offset = Pointer(image, i + 1);
    EXPECT_EQ(built->table[i].offset, offset);
    EXPECT_EQ(built->table[i].number, static_cast<int>(i + 1));
    const lpc::ParsedFrames parsed =
        FrameCodec::Parse(std::span<const std::uint8_t>(image).subspan(
            static_cast<std::size_t>(offset)));
    EXPECT_EQ(parsed.frames, words[i].frames);
    // One spare byte after the stop code's byte.
    if (i + 1 < words.size()) {
      EXPECT_EQ(Pointer(image, i + 2),
                offset + static_cast<int>((parsed.bits_used + 7) / 8) + 1);
    }
  }
}

TEST(TiIndexedTest, RejectsWordsThatDoNotFit) {
  std::mt19937 rng(11);
  const std::vector<Word> words(
      20, Word{.name = "W", .frames = SpeechLikeFrames(rng, 400)});
  const StatusOr<PhromImage> built = TiIndexed::Build(words);
  ASSERT_FALSE(built.IsOk());
  EXPECT_EQ(built.GetStatus().Code(), StatusCode::kResourceExhausted);
}

TEST(TiIndexedTest, RejectsMoreThan255Words) {
  const std::vector<Word> words(
      256, Word{.name = "W", .frames = {lpc::Frame::Stop()}});
  EXPECT_EQ(TiIndexed::Build(words).GetStatus().Code(),
            StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace speak5220::core::packaging
