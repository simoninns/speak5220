/*
 * File:        speak_external_stream_test.cpp
 * Module:      packaging tests
 * Purpose:     Unit tests of Speak External streams and the 9-byte rule
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/speak_external_stream.h"

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

using lpc::Frame;
using lpc::FrameCodec;

std::vector<std::uint8_t> BuildOrDie(const std::vector<Frame>& frames) {
  StatusOr<std::vector<std::uint8_t>> stream =
      SpeakExternalStream::Build(frames);
  EXPECT_TRUE(stream.IsOk());
  return stream.IsOk() ? *stream : std::vector<std::uint8_t>();
}

// Re-implements test_tms5220enc.py Packaging.testSpeakExternalMinimum.
TEST(SpeakExternalStreamTest, PadsShortPhrasesToNineBytesWithSilence) {
  std::mt19937 rng(8);
  const std::vector<std::vector<Frame>> cases = {
      {Frame::Stop()},
      {Frame::Unvoiced(3, {1, 2, 3, 4})},
      {speak5220::testing::RandomFrame(rng, lpc::FrameType::kVoiced)},
  };
  for (const std::vector<Frame>& frames : cases) {
    const std::vector<std::uint8_t> data = BuildOrDie(frames);
    // speak5220 datasheet §12.9: at least 9 bytes.
    EXPECT_GE(data.size(), SpeakExternalStream::kMinimumBytes);
    const lpc::ParsedFrames parsed = FrameCodec::Parse(data);
    EXPECT_EQ(data.size(), (parsed.bits_used + 7) / 8);
    ASSERT_FALSE(parsed.frames.empty());
    EXPECT_EQ(parsed.frames.back(), Frame::Stop());
    std::vector<Frame> body;
    for (const Frame& frame : frames) {
      if (frame.energy != lpc::kStopEnergy) {
        body.push_back(frame);
      }
    }
    ASSERT_GE(parsed.frames.size(), body.size() + 1);
    EXPECT_EQ(std::vector<Frame>(parsed.frames.begin(),
                                 parsed.frames.begin() +
                                     static_cast<std::ptrdiff_t>(body.size())),
              body);
    for (std::size_t i = body.size(); i + 1 < parsed.frames.size(); ++i) {
      EXPECT_EQ(parsed.frames[i], Frame::Silence());
    }
    // No more silence than needed.
    if (parsed.frames.size() > body.size() + 1) {
      std::vector<Frame> shorter(parsed.frames.begin(),
                                 parsed.frames.end() - 2);
      shorter.push_back(Frame::Stop());
      const StatusOr<std::vector<std::uint8_t>> packed =
          FrameCodec::Pack(shorter);
      ASSERT_TRUE(packed.IsOk());
      EXPECT_LT(packed->size(), SpeakExternalStream::kMinimumBytes);
    }
  }
}

TEST(SpeakExternalStreamTest, LongPhraseIsItsPackedFrames) {
  std::mt19937 rng(9);
  const std::vector<Frame> frames =
      speak5220::testing::SpeechLikeFrames(rng, 10);
  const StatusOr<std::vector<std::uint8_t>> packed = FrameCodec::Pack(frames);
  ASSERT_TRUE(packed.IsOk());
  EXPECT_EQ(BuildOrDie(frames), *packed);
}

}  // namespace
}  // namespace speak5220::core::packaging
