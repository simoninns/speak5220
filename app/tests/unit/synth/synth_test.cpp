/*
 * File:        synth_test.cpp
 * Module:      synth tests
 * Purpose:     Unit tests of Synth: the host command sequences of both
 *              paths, idle marking, errors and caching
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/synth.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/packaging/speak_external_stream.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/emulator_mocks.h"
#include "support/frame_builder.h"

namespace speak5220::core::synth {
namespace {

using lpc::Frame;
using speak5220::testing::kNeutralVoicedK;
using speak5220::testing::MockVsmBus;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

std::vector<std::uint8_t> Pack(const std::vector<Frame>& frames) {
  StatusOr<std::vector<std::uint8_t>> packed = lpc::FrameCodec::Pack(frames);
  EXPECT_TRUE(packed.IsOk());
  return packed.IsOk() ? *packed : std::vector<std::uint8_t>();
}

std::vector<Frame> VoicedPhrase() {
  return {Frame::Voiced(10, 30, kNeutralVoicedK),
          Frame::Voiced(10, 30, kNeutralVoicedK), Frame::Stop()};
}

TEST(SynthTest, VsmPathLoadsAddressThenDummyReadThenReadsFrame) {
  StrictMock<MockVsmBus> vsm;
  {
    // Load Address x 5 (0x12345, low nibble first), the Speak's Dummy
    // Read, then the first frame: a stop code, 1111.
    InSequence order;
    for (const int nibble : {5, 4, 3, 2, 1}) {
      EXPECT_CALL(vsm, Load(nibble));
    }
    EXPECT_CALL(vsm, Dummy());
    EXPECT_CALL(vsm, Read()).Times(4).WillRepeatedly(Return(1));
  }
  const Synth synth;
  const StatusOr<SynthResult> result = synth.SpeakFromVsm(vsm, 0x12345);
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  // A stop code as the first frame gives no output change.
  EXPECT_FALSE(result->codes.empty());
  EXPECT_EQ(result->leading_idle_samples, result->codes.size());
}

TEST(SynthTest, SpeakExternalPathUsesNoVsmFunction) {
  StrictMock<MockVsmBus> vsm;  // any call fails the test
  const std::array<Frame, 1> stop = {Frame::Stop()};
  const StatusOr<std::vector<std::uint8_t>> stream =
      packaging::SpeakExternalStream::Build(stop);
  ASSERT_TRUE(stream.IsOk());
  const Synth synth;
  const StatusOr<SynthResult> result = synth.SpeakExternal(*stream, vsm);
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  EXPECT_FALSE(result->codes.empty());
}

TEST(SynthTest, ShortSpeakExternalStreamNeverStarts) {
  StrictMock<MockVsmBus> vsm;
  const std::vector<std::uint8_t> eight(8, 0);  // eight silence-only bytes
  const Synth synth;
  const StatusOr<SynthResult> result = synth.SpeakExternal(eight, vsm);
  ASSERT_FALSE(result.IsOk());
  EXPECT_EQ(result.GetStatus().Code(), StatusCode::kFailedPrecondition);
}

TEST(SynthTest, MarksLeadingIdleAndScalesSamples) {
  Synth synth;
  const StatusOr<SynthResult> result =
      synth.Synthesize(Pack(VoicedPhrase()), SynthPath::kVsm);
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  // §7: the first sample that differs from idle is 206-207 samples after
  // the first frame read.
  EXPECT_GE(result->leading_idle_samples, 205U);
  EXPECT_LE(result->leading_idle_samples, 208U);
  for (std::size_t i = 0; i < result->leading_idle_samples; ++i) {
    ASSERT_EQ(result->codes[i], -1);
  }
  EXPECT_NE(result->codes[result->leading_idle_samples], -1);
  ASSERT_EQ(result->samples.size(), result->codes.size());
  for (std::size_t i = 0; i < result->codes.size(); ++i) {
    ASSERT_EQ(result->samples[i], result->codes[i] * 64);
  }
  // The standard procedure reads the first frame at sample period 200.
  EXPECT_EQ(result->first_period, 200);
}

TEST(SynthTest, SpeakExternalStartsTwoSamplesEarlier) {
  Synth synth;
  const std::vector<Frame> frames = VoicedPhrase();
  const StatusOr<std::vector<std::uint8_t>> stream =
      packaging::SpeakExternalStream::Build(frames);
  ASSERT_TRUE(stream.IsOk());
  const StatusOr<SynthResult> vsm =
      synth.Synthesize(Pack(frames), SynthPath::kVsm);
  const StatusOr<SynthResult> external =
      synth.Synthesize(*stream, SynthPath::kSpeakExternal);
  ASSERT_TRUE(vsm.IsOk());
  ASSERT_TRUE(external.IsOk()) << external.GetStatus().ToString();
  // §8.5: measured from the same command latch, Speak External's output
  // starts 2 samples before Speak's; its first frame read leads by 2.
  EXPECT_EQ(external->leading_idle_samples, vsm->leading_idle_samples);
}

TEST(SynthTest, RejectsEmptyPhrase) {
  Synth synth;
  const StatusOr<SynthResult> result = synth.Synthesize({}, SynthPath::kVsm);
  ASSERT_FALSE(result.IsOk());
  EXPECT_EQ(result.GetStatus().Code(), StatusCode::kInvalidArgument);
}

TEST(SynthTest, CachesEachPhraseAndPathOnce) {
  Synth synth;
  const std::vector<std::uint8_t> phrase = Pack(VoicedPhrase());
  const StatusOr<SynthResult> first = synth.Synthesize(phrase, SynthPath::kVsm);
  const StatusOr<SynthResult> again = synth.Synthesize(phrase, SynthPath::kVsm);
  ASSERT_TRUE(first.IsOk());
  ASSERT_TRUE(again.IsOk());
  EXPECT_EQ(first->codes, again->codes);
  EXPECT_EQ(synth.CacheSize(), 1U);
  std::vector<std::uint8_t> padded = phrase;
  padded.resize(16, 0);
  ASSERT_TRUE(synth.Synthesize(padded, SynthPath::kSpeakExternal).IsOk());
  EXPECT_EQ(synth.CacheSize(), 2U);
}

TEST(SynthTest, KeepsNothingWithTheCacheOff) {
  Synth synth({}, SynthCache::kOff);
  const std::vector<std::uint8_t> phrase = Pack(VoicedPhrase());
  const StatusOr<SynthResult> first = synth.Synthesize(phrase, SynthPath::kVsm);
  const StatusOr<SynthResult> again = synth.Synthesize(phrase, SynthPath::kVsm);
  ASSERT_TRUE(first.IsOk());
  ASSERT_TRUE(again.IsOk());
  EXPECT_EQ(first->codes, again->codes);
  EXPECT_EQ(synth.CacheSize(), 0U);
}

}  // namespace
}  // namespace speak5220::core::synth
