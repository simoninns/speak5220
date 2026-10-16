/*
 * File:        encoded_audio_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of RenderEncodedAudio() with a mocked synth:
 *              lining the output up with the source, and the fallbacks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/encoded_audio.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/lpc/frame.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/synth_mocks.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

// No source audio.
constexpr std::span<const float> kNoSource;

using ::testing::_;
using ::testing::Return;

// Where the speech starts in the chip's output, in samples.
constexpr int kSpeechStart = 300;

// The synthetic speech as chip codes, `lead` idle codes (-1) first.
synth::SynthResult SpokenAsCodes(const std::vector<double>& x, int lead) {
  synth::SynthResult r;
  r.codes.assign(static_cast<std::size_t>(lead), -1);
  for (const double v : x) {
    r.codes.push_back(static_cast<int>(std::lround(v * 200.0)));
  }
  r.codes.insert(r.codes.end(), 300, -1);
  for (const int code : r.codes) {
    r.samples.push_back(static_cast<std::int16_t>(code * 64));
  }
  return r;
}

const std::vector<lpc::Frame> kFrames = {lpc::Frame::Voiced(10, 30, {}),
                                         lpc::Frame::Stop()};

TEST(EncodedAudioTest, LinesTheOutputUpWithTheSource) {
  const std::vector<double> x = testing::SyntheticSpeech();
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, synth::SynthPath::kVsm))
      .WillOnce(Return(SpokenAsCodes(x, kSpeechStart)));
  const StatusOr<EncodedAudio> audio =
      RenderEncodedAudio(synth, dsp::ToFloat(x), kFrames);
  ASSERT_TRUE(audio.IsOk()) << audio.GetStatus().ToString();
  // Within one step of the refinement (0.625 ms, 5 samples): the source
  // is compared after the 60 Hz high-pass.
  EXPECT_NEAR(audio->source_offset, kSpeechStart, 5);
  EXPECT_EQ(audio->samples, SpokenAsCodes(x, kSpeechStart).samples);
  // The codes are the source x 200, and a sample is a code x 64 of 32768:
  // the source's level is the samples' x 32768 / (200 x 64) = 2.56 (the
  // codes are rounded, and idle sits one code below 0).
  EXPECT_NEAR(audio->level_gain, 2.56, 0.03);
}

TEST(EncodedAudioTest, NoSourcePutsTheFirstFrameAtTheStart) {
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, _))
      .WillOnce(
          Return(SpokenAsCodes(testing::SyntheticSpeech(), kSpeechStart)));
  const StatusOr<EncodedAudio> audio =
      RenderEncodedAudio(synth, kNoSource, kFrames);
  ASSERT_TRUE(audio.IsOk());
  EXPECT_EQ(audio->source_offset, kFirstFrameStretch);
  EXPECT_EQ(audio->level_gain, 1.0);
}

TEST(EncodedAudioTest, SilentOutputPutsTheFirstFrameAtTheStart) {
  synth::SynthResult silent;
  silent.codes.assign(400, -1);
  silent.samples.assign(400, -64);
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, _)).WillOnce(Return(silent));
  const StatusOr<EncodedAudio> audio = RenderEncodedAudio(
      synth, dsp::ToFloat(testing::SyntheticSpeech()), kFrames);
  ASSERT_TRUE(audio.IsOk());
  EXPECT_EQ(audio->source_offset, kFirstFrameStretch);
  EXPECT_EQ(audio->samples.size(), 400U);
  EXPECT_EQ(audio->level_gain, 1.0);
}

TEST(EncodedAudioTest, SynthFailureIsReturned) {
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, _))
      .WillOnce(Return(InternalError("the Speak command did not start")));
  EXPECT_EQ(RenderEncodedAudio(synth, kNoSource, kFrames).GetStatus().Code(),
            StatusCode::kInternal);
}

TEST(EncodedAudioTest, InvalidFrameIsRefusedBeforeSpeaking) {
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, _)).Times(0);
  const std::vector<lpc::Frame> bad = {lpc::Frame{.energy = 99}};
  EXPECT_FALSE(RenderEncodedAudio(synth, kNoSource, bad).IsOk());
}

}  // namespace
}  // namespace speak5220::core::quality
