/*
 * File:        frame_statistics_test.cpp
 * Module:      lpc tests
 * Purpose:     Unit tests of the frame counts, bits, bytes and bit rate of
 *              a phrase
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/lpc/frame_statistics.h"

#include <vector>

#include "core/lpc/frame.h"
#include "gtest/gtest.h"

namespace speak5220::core::lpc {
namespace {

TEST(FrameStatisticsTest, CountsEachKindAndTheBitsUpToTheStop) {
  const std::vector<Frame> frames = {
      Frame::Silence(),
      Frame::Voiced(5, 20, {1, 2, 3, 4, 5, 6, 7, 1, 2, 3}),
      Frame::Repeat(6, 20),
      Frame::Unvoiced(4, {1, 2, 3, 4}),
      Frame::Stop(),
      // After the stop: not counted.
      Frame::Silence(),
  };
  const FrameStatistics stats = SummariseFrames(frames);
  EXPECT_EQ(stats.frames, 4);
  EXPECT_EQ(stats.silence, 1);
  EXPECT_EQ(stats.voiced, 1);
  EXPECT_EQ(stats.repeat, 1);
  EXPECT_EQ(stats.unvoiced, 1);
  // 4 + 50 + 11 + 29 + 4 (the stop) bits.
  EXPECT_EQ(stats.bits, 98);
  EXPECT_EQ(stats.bytes, 13);
  EXPECT_DOUBLE_EQ(stats.Seconds(), 0.1);
  EXPECT_DOUBLE_EQ(stats.BitRate(), 980.0);
}

TEST(FrameStatisticsTest, CountsAStopWhenThereIsNone) {
  const std::vector<Frame> frames = {Frame::Silence(), Frame::Silence()};
  const FrameStatistics stats = SummariseFrames(frames);
  EXPECT_EQ(stats.frames, 2);
  EXPECT_EQ(stats.bits, 12);
  EXPECT_EQ(stats.bytes, 2);
}

TEST(FrameStatisticsTest, NoFramesHaveNoBitRate) {
  const std::vector<Frame> frames = {Frame::Stop()};
  const FrameStatistics stats = SummariseFrames(frames);
  EXPECT_EQ(stats.frames, 0);
  EXPECT_EQ(stats.bytes, 1);
  EXPECT_DOUBLE_EQ(stats.BitRate(), 0.0);
}

}  // namespace
}  // namespace speak5220::core::lpc
