/*
 * File:        frame_map_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of which frame each encoded sample belongs to,
 *              and of the frames' hover text
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/frame_map.h"

#include <QString>
#include <optional>

#include "core/lpc/frame.h"
#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

using core::lpc::Frame;

TEST(FrameMapTest, StretchesStartWhereEachFrameIsHalfWayReached) {
  EXPECT_EQ(FrameStretchStart(0), 125);
  EXPECT_EQ(FrameStretchStart(1), 325);
  EXPECT_EQ(FrameStretchStart(10), 2125);
}

TEST(FrameMapTest, MapsEachEncodedSampleToItsFrame) {
  constexpr int kFrames = 3;
  EXPECT_EQ(FrameAtEncodedSample(-1, kFrames), std::nullopt);
  EXPECT_EQ(FrameAtEncodedSample(124, kFrames), std::nullopt);
  EXPECT_EQ(FrameAtEncodedSample(125, kFrames), 0);
  EXPECT_EQ(FrameAtEncodedSample(324, kFrames), 0);
  EXPECT_EQ(FrameAtEncodedSample(325, kFrames), 1);
  EXPECT_EQ(FrameAtEncodedSample(724, kFrames), 2);
  EXPECT_EQ(FrameAtEncodedSample(725, kFrames), std::nullopt);
}

TEST(FrameMapTest, DescribesEachKindWithTheIndicesItCarries) {
  EXPECT_EQ(
      DescribeFrame(12, Frame::Voiced(9, 31, {20, 15, 9, 8, 7, 6, 5, 4, 3, 2})),
      QStringLiteral("Frame 12: voiced, energy 9, pitch 31, "
                     "K 20 15 9 8 7 6 5 4 3 2"));
  EXPECT_EQ(DescribeFrame(3, Frame::Unvoiced(5, {1, 2, 3, 4})),
            QStringLiteral("Frame 3: unvoiced, energy 5, K 1 2 3 4"));
  EXPECT_EQ(DescribeFrame(4, Frame::Repeat(7, 40)),
            QStringLiteral("Frame 4: repeat, energy 7, pitch 40"));
  EXPECT_EQ(DescribeFrame(0, Frame::Silence()),
            QStringLiteral("Frame 0: silence"));
  EXPECT_EQ(DescribeFrame(5, Frame::Stop()), QStringLiteral("Frame 5: stop"));
}

}  // namespace
}  // namespace speak5220::app
