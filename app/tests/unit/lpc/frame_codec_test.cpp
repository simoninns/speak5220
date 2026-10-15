/*
 * File:        frame_codec_test.cpp
 * Module:      lpc tests
 * Purpose:     Unit tests of frame kinds, lengths, packing, parsing and the
 *              chip's bit order
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/lpc/frame_codec.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <span>
#include <vector>

#include "core/lpc/frame.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::lpc {
namespace {

using speak5220::testing::RandomFrame;

std::vector<std::uint8_t> PackOrDie(std::span<const Frame> frames) {
  StatusOr<std::vector<std::uint8_t>> packed = FrameCodec::Pack(frames);
  EXPECT_TRUE(packed.IsOk()) << packed.GetStatus().ToString();
  return packed.IsOk() ? *packed : std::vector<std::uint8_t>();
}

std::size_t TotalBits(std::span<const Frame> frames) {
  std::size_t bits = 0;
  for (const Frame& frame : frames) {
    bits += static_cast<std::size_t>(FrameBits(frame));
  }
  return bits;
}

// The data of the word "ZERO" in the Acorn Speech System User Guide,
// Section 5 ("Loading speech data for the word ZERO into memory"), without
// its terminating 0. It is in the order a VSM read gives, as stored in a
// PHROM: the guide bit-reverses every byte before sending it to Speak
// External.
constexpr std::array<std::uint8_t, 143> kGuideZero = {
    69,  212, 4,   180, 85,  88,  85,  109, 129, 43,  17,  78,  53,  1,   241,
    35,  44,  9,   85,  241, 34,  171, 194, 178, 211, 104, 204, 49,  109, 34,
    196, 89,  59,  132, 229, 214, 181, 6,   20,  193, 57,  121, 174, 65,  150,
    72,  46,  77,  139, 143, 225, 46,  85,  145, 98,  228, 24,  108, 156, 58,
    232, 185, 6,   21,  73,  44,  218, 45,  69,  101, 83,  75,  190, 139, 17,
    206, 109, 79,  21,  105, 96,  179, 193, 178, 196, 186, 237, 52,  240, 46,
    207, 19,  59,  81,  58,  149, 37,  220, 170, 213, 206, 167, 71,  92,  35,
    182, 52,  146, 209, 188, 246, 169, 141, 30,  178, 235, 30,  41,  101, 38,
    195, 204, 71,  74,  89,  73,  177, 16,  207, 210, 134, 82,  140, 59,  29,
    100, 221, 164, 160, 238, 169, 94,  47};

// Re-implements test_tms5220enc.py PackParse.testFrameLengths.
TEST(FrameCodecTest, EachKindHasItsLength) {
  std::mt19937 rng(1);
  const std::array<std::pair<FrameType, int>, 5> kinds = {{
      {FrameType::kVoiced, 50},
      {FrameType::kUnvoiced, 29},
      {FrameType::kRepeat, 11},
      {FrameType::kSilence, 4},
      {FrameType::kStop, 4},
  }};
  for (const auto& [type, bits] : kinds) {
    const Frame frame = RandomFrame(rng, type);
    EXPECT_EQ(FrameBits(frame), bits);
    EXPECT_EQ(FrameKind(frame), type);
    const std::array<Frame, 2> stream = {frame, Frame::Stop()};
    const ParsedFrames parsed = FrameCodec::Parse(PackOrDie(stream));
    EXPECT_EQ(parsed.bits_used, static_cast<std::size_t>(
                                    bits + (type == FrameType::kStop ? 0 : 4)));
  }
}

// Re-implements test_tms5220enc.py PackParse.testRoundTripRandom.
TEST(FrameCodecTest, RandomFramesRoundTrip) {
  std::mt19937 rng(2);
  for (int trial = 0; trial < 300; ++trial) {
    const int count = std::uniform_int_distribution<int>(0, 40)(rng);
    std::vector<Frame> frames;
    frames.reserve(static_cast<std::size_t>(count) + 1);
    for (int i = 0; i < count; ++i) {
      frames.push_back(RandomFrame(rng));
    }
    frames.push_back(Frame::Stop());
    const std::vector<std::uint8_t> data = PackOrDie(frames);
    const ParsedFrames parsed = FrameCodec::Parse(data);
    ASSERT_EQ(parsed.frames, frames) << "trial " << trial;
    EXPECT_EQ(parsed.bits_used, TotalBits(frames));
    EXPECT_EQ(data.size(), (parsed.bits_used + 7) / 8);
    // The padding after the stop code is zero.
    if (parsed.bits_used % 8 != 0) {
      EXPECT_EQ(data.back() >> (parsed.bits_used % 8), 0);
    }
  }
}

// Re-implements test_tms5220enc.py PackParse.testRoundTripHandMade.
TEST(FrameCodecTest, HandMadeFramesRoundTrip) {
  const std::vector<Frame> frames = {
      Frame::Voiced(14, 63, {31, 31, 15, 15, 15, 15, 15, 7, 7, 7}),
      Frame::Voiced(1, 1, {0, 0, 0, 0, 0, 0, 0, 0, 0, 0}),
      Frame::Repeat(7, 40),
      Frame::Silence(),
      Frame::Unvoiced(5, {3, 30, 1, 14}),
      Frame::Repeat(3, 0),
      Frame::Stop(),
  };
  const ParsedFrames parsed = FrameCodec::Parse(PackOrDie(frames));
  EXPECT_EQ(parsed.frames, frames);
  EXPECT_EQ(parsed.bits_used, 50U + 50 + 11 + 4 + 29 + 11 + 4);
}

TEST(FrameCodecTest, PackingStopsAtStopFrame) {
  const std::vector<Frame> frames = {Frame::Silence(), Frame::Stop(),
                                     Frame::Silence()};
  EXPECT_EQ(PackOrDie(frames),
            PackOrDie(std::span<const Frame>(frames).first(2)));
}

// Re-implements test_tms5220enc.py PackParse.testTruncated.
TEST(FrameCodecTest, TruncatedStreamGivesWholeFramesOnly) {
  std::mt19937 rng(3);
  std::vector<Frame> frames;
  frames.reserve(4);
  for (int i = 0; i < 3; ++i) {
    frames.push_back(RandomFrame(rng, FrameType::kVoiced));
  }
  frames.push_back(Frame::Stop());
  const std::vector<std::uint8_t> data = PackOrDie(frames);
  // 96 bits: one whole frame, then part of one.
  const ParsedFrames parsed =
      FrameCodec::Parse(std::span<const std::uint8_t>(data).first(12));
  EXPECT_EQ(parsed.frames,
            std::vector<Frame>(frames.begin(), frames.begin() + 1));
  EXPECT_EQ(parsed.bits_used, 50U);
  const ParsedFrames limited = FrameCodec::Parse(data, 2);
  EXPECT_EQ(limited.frames,
            std::vector<Frame>(frames.begin(), frames.begin() + 2));
}

// Re-implements test_tms5220enc.py PackParse.testInvalid.
TEST(FrameCodecTest, RejectsInvalidFields) {
  Frame energy_too_big;
  energy_too_big.energy = 16;
  Frame unvoiced_with_k5 = Frame::Unvoiced(5, {0, 0, 0, 0});
  unvoiced_with_k5.k[4] = 1;
  Frame k1_too_big = Frame::Voiced(5, 3, {32, 0, 0, 0, 0, 0, 0, 0, 0, 0});
  Frame pitch_too_big = Frame::Repeat(5, 64);
  Frame silence_with_pitch = Frame::Silence();
  silence_with_pitch.pitch = 3;
  for (const Frame& frame : {energy_too_big, unvoiced_with_k5, k1_too_big,
                             pitch_too_big, silence_with_pitch}) {
    const std::array<Frame, 1> one = {frame};
    const StatusOr<std::vector<std::uint8_t>> packed = FrameCodec::Pack(one);
    ASSERT_FALSE(packed.IsOk());
    EXPECT_EQ(packed.GetStatus().Code(), StatusCode::kInvalidArgument);
  }
}

// Re-implements test_tms5220enc.py BitOrder.testStopByte: 1111 read LSB
// first from the byte.
TEST(FrameCodecTest, PacksStopFrameAsFourBits) {
  const std::array<Frame, 1> stop = {Frame::Stop()};
  EXPECT_EQ(PackOrDie(stop), std::vector<std::uint8_t>({0x0F}));
}

// Re-implements test_tms5220enc.py BitOrder.testFieldsMsbFirst.
TEST(FrameCodecTest, WritesFieldsMsbFirstIntoBytesLsbFirst) {
  // Energy 1 = 0001, repeat 1, pitch 1 = 000001, then stop 1111: stream
  // bits 0001 1 000001 1111 fill bytes from bit 0.
  const std::array<Frame, 2> frames = {Frame::Repeat(1, 1), Frame::Stop()};
  EXPECT_EQ(PackOrDie(frames), std::vector<std::uint8_t>({0x18, 0x7C}));
  // Energy 8 (1000): bit 0 of the first byte is the energy's MSB.
  const std::array<Frame, 3> eight = {Frame::Repeat(8, 0), Frame::Silence(),
                                      Frame::Stop()};
  EXPECT_EQ(PackOrDie(eight).front() & 1, 1);
}

TEST(FrameCodecTest, MatchesHandAssembledBitStream) {
  speak5220::testing::BitStream stream;
  stream.Voiced(10, 30, speak5220::testing::kNeutralVoicedK);
  stream.Repeat(10, 30);
  stream.Unvoiced(8, speak5220::testing::kNeutralUnvoicedK);
  stream.Silence();
  stream.Stop();
  const std::vector<Frame> frames = {
      Frame::Voiced(10, 30, speak5220::testing::kNeutralVoicedK),
      Frame::Repeat(10, 30),
      Frame::Unvoiced(8, speak5220::testing::kNeutralUnvoicedK),
      Frame::Silence(), Frame::Stop()};
  EXPECT_EQ(PackOrDie(frames), stream.Bytes());
}

// speak5220 datasheet §6 (re-implements test_tms5220enc.py
// BitOrder.testReadByteReversal): image bytes 0x23, 0x9B, 0x35, 0x85 read
// back as 0xC4, 0xD9, 0xAC, 0xA1.
TEST(BitReverseTest, MatchesReadByteExamples) {
  EXPECT_EQ(BitReverse(0x23), 0xC4);
  EXPECT_EQ(BitReverse(0x9B), 0xD9);
  EXPECT_EQ(BitReverse(0x35), 0xAC);
  EXPECT_EQ(BitReverse(0x85), 0xA1);
  for (int b = 0; b < 256; ++b) {
    const auto byte = static_cast<std::uint8_t>(b);
    EXPECT_EQ(BitReverse(BitReverse(byte)), byte);
  }
}

// The guide's "ZERO" data parses only after bit reversal: 25 frames,
// with the stop code in the last of its 143 bytes.
TEST(FrameCodecTest, GuideZeroParsesAfterBitReversal) {
  std::vector<std::uint8_t> reversed;
  reversed.reserve(kGuideZero.size());
  for (const std::uint8_t byte : kGuideZero) {
    reversed.push_back(BitReverse(byte));
  }
  const ParsedFrames parsed = FrameCodec::Parse(reversed);
  ASSERT_EQ(parsed.frames.size(), 25U);
  EXPECT_EQ(parsed.frames.back(), Frame::Stop());
  EXPECT_EQ((parsed.bits_used + 7) / 8, 143U);
  // The guide's first frame: energy 0100, repeat 0, pitch 101110, then K1
  // to K10 as listed in its Section 7.
  EXPECT_EQ(parsed.frames.front(),
            Frame::Voiced(4, 46, {20, 0, 9, 6, 8, 10, 10, 5, 4, 1}));
  EXPECT_EQ(PackOrDie(parsed.frames), reversed);
}

TEST(FrameCodecTest, GuideZeroDoesNotParseUnreversed) {
  const ParsedFrames parsed = FrameCodec::Parse(kGuideZero);
  // A stop code is found, but after 89 bytes rather than at the end.
  EXPECT_EQ((parsed.bits_used + 7) / 8, 89U);
}

TEST(FrameTest, EnsureStopEndsAtFirstStop) {
  const std::vector<Frame> frames = {Frame::Silence(), Frame::Stop(),
                                     Frame::Repeat(3, 4)};
  EXPECT_EQ(EnsureStop(frames),
            std::vector<Frame>({Frame::Silence(), Frame::Stop()}));
  const std::vector<Frame> open = {Frame::Repeat(3, 4)};
  EXPECT_EQ(EnsureStop(open),
            std::vector<Frame>({Frame::Repeat(3, 4), Frame::Stop()}));
}

}  // namespace
}  // namespace speak5220::core::lpc
