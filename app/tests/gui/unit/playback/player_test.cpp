/*
 * File:        player_test.cpp
 * Module:      playback tests
 * Purpose:     Unit tests of the player with a mocked audio output and a
 *              fake clock: play, pause, resume, the playhead and the end
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/playback/player.h"

#include <cstdint>
#include <numeric>
#include <vector>

#include "core/dsp/output_filter.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/support/gui_mocks.h"
#include "support/project_mocks.h"

namespace speak5220::app {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Eq;
using testing::FakeClock;
using ::testing::InSequence;
using testing::MockAudioOutput;
using testing::MockResampler;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::SizeIs;

constexpr std::int64_t kMillisecond = 1000;

// 8000 samples numbered 0, 1, 2, ...: one second.
std::vector<std::int16_t> Numbered() {
  std::vector<std::int16_t> samples(8000);
  std::iota(samples.begin(), samples.end(), std::int16_t{0});
  return samples;
}

class PlayerTest : public ::testing::Test {
 protected:
  PlayerTest() {
    ON_CALL(output_, PlaybackRate(_)).WillByDefault(::testing::ReturnArg<0>());
    ON_CALL(output_, Start(_, _)).WillByDefault(Return(core::OkStatus()));
    ON_CALL(output_, IsFinished()).WillByDefault(Return(false));
  }

  NiceMock<MockAudioOutput> output_;
  FakeClock clock_;
  NiceMock<MockResampler> resampler_;
  Player player_{output_, clock_, resampler_};
};

TEST_F(PlayerTest, PlaysFromTheStart) {
  EXPECT_CALL(output_, Start(SizeIs(8000 - 1200), Player::kSampleRate))
      .WillOnce([](const std::vector<std::int16_t>& samples, int) {
        EXPECT_EQ(samples.front(), 1200);
        return core::OkStatus();
      });
  ASSERT_TRUE(player_.Play(PlaybackSource::kEncoded, Numbered(), 1200).IsOk());
  EXPECT_EQ(player_.State(), PlaybackState::kPlaying);
  EXPECT_EQ(player_.Source(), PlaybackSource::kEncoded);
  EXPECT_EQ(player_.Position(), 1200);
}

TEST_F(PlayerTest, PlayheadIsTheTimePlayedButNoMoreThanTheDeviceTook) {
  clock_.now_us = 5000 * kMillisecond;
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 400).IsOk());
  // 100 ms played; the device has taken 200 ms: 800 samples on.
  clock_.now_us += 100 * kMillisecond;
  EXPECT_CALL(output_, ProcessedMicroseconds())
      .WillRepeatedly(Return(200 * kMillisecond));
  player_.Tick();
  EXPECT_EQ(player_.Position(), 400 + 800);
  // 300 ms played, but the device has taken only 200 ms.
  clock_.now_us += 200 * kMillisecond;
  player_.Tick();
  EXPECT_EQ(player_.Position(), 400 + 1600);
}

TEST_F(PlayerTest, PositionChangesAreSignalled) {
  std::vector<int> positions;
  QObject::connect(
      &player_, &Player::PositionChanged,
      [&positions](int position) { positions.push_back(position); });
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 0).IsOk());
  clock_.now_us += 50 * kMillisecond;
  EXPECT_CALL(output_, ProcessedMicroseconds())
      .WillRepeatedly(Return(50 * kMillisecond));
  player_.Tick();
  EXPECT_THAT(positions, ElementsAre(0, 400));
}

TEST_F(PlayerTest, EndStopsAndReturnsToTheStart) {
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 400).IsOk());
  EXPECT_CALL(output_, IsFinished()).WillOnce(Return(true));
  player_.Tick();
  EXPECT_EQ(player_.State(), PlaybackState::kStopped);
  EXPECT_EQ(player_.Position(), 400);
}

TEST_F(PlayerTest, PlayheadPastTheLastSampleStops) {
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 0).IsOk());
  clock_.now_us += 2000 * kMillisecond;
  EXPECT_CALL(output_, ProcessedMicroseconds())
      .WillRepeatedly(Return(2000 * kMillisecond));
  player_.Tick();
  EXPECT_EQ(player_.State(), PlaybackState::kStopped);
}

TEST_F(PlayerTest, PauseHoldsThePositionAndResumeGoesOnFromIt) {
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 400).IsOk());
  clock_.now_us += 250 * kMillisecond;
  EXPECT_CALL(output_, ProcessedMicroseconds())
      .WillRepeatedly(Return(250 * kMillisecond));
  EXPECT_CALL(output_, Stop());
  player_.Pause();
  EXPECT_EQ(player_.State(), PlaybackState::kPaused);
  EXPECT_EQ(player_.Position(), 400 + 2000);
  ::testing::Mock::VerifyAndClearExpectations(&output_);

  EXPECT_CALL(output_, Start(SizeIs(8000 - 2400), _))
      .WillOnce(Return(core::OkStatus()));
  ASSERT_TRUE(player_.Resume().IsOk());
  EXPECT_EQ(player_.State(), PlaybackState::kPlaying);
  // The playhead counts from where it resumed.
  clock_.now_us += 100 * kMillisecond;
  EXPECT_CALL(output_, ProcessedMicroseconds())
      .WillRepeatedly(Return(100 * kMillisecond));
  player_.Tick();
  EXPECT_EQ(player_.Position(), 2400 + 800);
}

TEST_F(PlayerTest, StopReturnsToTheStart) {
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 400).IsOk());
  clock_.now_us += 100 * kMillisecond;
  EXPECT_CALL(output_, ProcessedMicroseconds())
      .WillRepeatedly(Return(100 * kMillisecond));
  player_.Tick();
  EXPECT_CALL(output_, Stop());
  player_.Stop();
  EXPECT_EQ(player_.State(), PlaybackState::kStopped);
  EXPECT_EQ(player_.Position(), 400);
}

TEST_F(PlayerTest, PlayingAgainStopsWhatIsPlaying) {
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 0).IsOk());
  {
    InSequence sequence;
    EXPECT_CALL(output_, Stop());
    EXPECT_CALL(output_, Start(_, _)).WillOnce(Return(core::OkStatus()));
  }
  ASSERT_TRUE(player_.Play(PlaybackSource::kEncoded, Numbered(), 0).IsOk());
  EXPECT_EQ(player_.Source(), PlaybackSource::kEncoded);
  ::testing::Mock::VerifyAndClearExpectations(&output_);
}

TEST_F(PlayerTest, StartAtOrPastTheEndPlaysFromTheBeginning) {
  EXPECT_CALL(output_, Start(SizeIs(8000), _))
      .WillOnce(Return(core::OkStatus()));
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 8000).IsOk());
  EXPECT_EQ(player_.Position(), 0);
}

TEST_F(PlayerTest, ResamplesForADeviceThatRefuses8kHz) {
  EXPECT_CALL(output_, PlaybackRate(Player::kSampleRate))
      .WillRepeatedly(Return(48000));
  EXPECT_CALL(resampler_, Resample(SizeIs(8000), Player::kSampleRate, 48000))
      .WillOnce(Return(std::vector<float>(48000, 0.5F)));
  EXPECT_CALL(output_, Start(SizeIs(48000), 48000))
      .WillOnce([](const std::vector<std::int16_t>& samples, int) {
        EXPECT_EQ(samples.front(), 16384);
        return core::OkStatus();
      });
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 0).IsOk());
  // The playhead is still in 8 kHz samples.
  clock_.now_us += 100 * kMillisecond;
  EXPECT_CALL(output_, ProcessedMicroseconds())
      .WillRepeatedly(Return(100 * kMillisecond));
  player_.Tick();
  EXPECT_EQ(player_.Position(), 800);
}

TEST_F(PlayerTest, DeviceFailureLeavesThePlayerStopped) {
  EXPECT_CALL(output_, Start(_, _))
      .WillOnce(Return(core::UnavailableError("there is no audio output")));
  EXPECT_FALSE(player_.Play(PlaybackSource::kOriginal, Numbered(), 0).IsOk());
  EXPECT_EQ(player_.State(), PlaybackState::kStopped);
}

TEST_F(PlayerTest, NothingToPlayIsRefused) {
  EXPECT_CALL(output_, Start(_, _)).Times(0);
  EXPECT_FALSE(player_.Play(PlaybackSource::kOriginal, {}, 0).IsOk());
  EXPECT_EQ(player_.State(), PlaybackState::kStopped);
}

TEST(ToPcm16Test, RoundsAndClips) {
  const std::vector<float> samples = {0.0F,  0.5F, -0.5F,           1.0F,
                                      -1.0F, 2.0F, 0.25F / 32768.0F};
  EXPECT_THAT(ToPcm16(samples),
              ElementsAre(0, 16384, -16384, 32767, -32768, 32767, 0));
}

TEST_F(PlayerTest, OutputFilterPlaysTheEncodedAudioAt48Kilohertz) {
  player_.SetOutputFilter(core::dsp::OutputFilter::kBbcMicro);
  EXPECT_CALL(output_, PlaybackRate(core::dsp::kOutputRate));
  EXPECT_CALL(output_,
              Start(SizeIs((8000 - 1200) * 6), core::dsp::kOutputRate));
  ASSERT_TRUE(player_.Play(PlaybackSource::kEncoded, Numbered(), 1200).IsOk());
  // The playhead still counts 8 kHz samples.
  EXPECT_EQ(player_.Position(), 1200);
}

TEST_F(PlayerTest, OutputFilterLeavesTheOriginalAlone) {
  player_.SetOutputFilter(core::dsp::OutputFilter::kBbcMicro);
  EXPECT_CALL(output_, Start(SizeIs(8000), Player::kSampleRate));
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 0).IsOk());
}

TEST_F(PlayerTest, OutputFilterIsResampledToTheDevicesRate) {
  player_.SetOutputFilter(core::dsp::OutputFilter::kBbcMicro);
  EXPECT_CALL(output_, PlaybackRate(core::dsp::kOutputRate))
      .WillOnce(Return(44100));
  EXPECT_CALL(resampler_,
              Resample(SizeIs(8000 * 6), core::dsp::kOutputRate, 44100))
      .WillOnce(Return(std::vector<float>(44100)));
  EXPECT_CALL(output_, Start(SizeIs(44100), 44100));
  ASSERT_TRUE(player_.Play(PlaybackSource::kOptimised, Numbered(), 0).IsOk());
}

TEST_F(PlayerTest, ChangingTheOutputFilterStopsTheEncodedAudio) {
  ASSERT_TRUE(player_.Play(PlaybackSource::kEncoded, Numbered(), 0).IsOk());
  player_.SetOutputFilter(core::dsp::OutputFilter::kBbcMicro);
  EXPECT_EQ(player_.State(), PlaybackState::kStopped);
}

TEST_F(PlayerTest, ChangingTheOutputFilterKeepsTheOriginalPlaying) {
  ASSERT_TRUE(player_.Play(PlaybackSource::kOriginal, Numbered(), 0).IsOk());
  player_.SetOutputFilter(core::dsp::OutputFilter::kBbcMicro);
  EXPECT_EQ(player_.State(), PlaybackState::kPlaying);
}

}  // namespace
}  // namespace speak5220::app
