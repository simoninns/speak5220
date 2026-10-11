/*
 * File:        frame_endings_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of frames from the plan: repeats, the K5-K10
 *              guard, fades and the stop
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/frame_endings.h"

#include <array>
#include <cstddef>
#include <cstdlib>
#include <span>
#include <vector>

#include "core/encoder/encoder_types.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

using lpc::Frame;

const std::vector<int> kK10 = {20, 15, 8, 8, 8, 8, 8, 4, 4, 4};
const std::vector<int> kK4 = {20, 15, 8, 8};
constexpr std::array<int, 10> kK10Array = {20, 15, 8, 8, 8, 8, 8, 4, 4, 4};
constexpr std::array<int, 4> kK4Array = {20, 15, 8, 8};

PlanEntry Voiced() {
  return {.kind = PlanKind::kVoiced, .k = kK10, .pitch = 30};
}
PlanEntry VoicedRepeat() {
  return {.kind = PlanKind::kRepeat, .k = kK10, .pitch = 30};
}
PlanEntry Unvoiced() {
  return {.kind = PlanKind::kUnvoiced, .k = kK4, .pitch = 0};
}
PlanEntry Silence() { return {}; }

TEST(FrameEndingsTest, RepeatsFollowTheirFullFrame) {
  const std::vector<PlanEntry> plan = {Voiced(), VoicedRepeat()};
  EXPECT_EQ(ToFrames(plan, std::vector<int>{10, 9}),
            (std::vector<Frame>{Frame::Voiced(10, 30, kK10Array),
                                Frame::Repeat(9, 30)}));
}

TEST(FrameEndingsTest, RepeatAfterASilencedFrameIsSentInFull) {
  // The full frame's energy became 0, so the chip never held its K.
  const std::vector<PlanEntry> plan = {Unvoiced(), Voiced(), VoicedRepeat()};
  EXPECT_EQ(ToFrames(plan, std::vector<int>{5, 0, 9}),
            (std::vector<Frame>{Frame::Unvoiced(5, kK4Array), Frame::Silence(),
                                Frame::Voiced(9, 30, kK10Array)}));
}

TEST(FrameEndingsTest, LeadingAndTrailingSilenceIsDropped) {
  const std::vector<PlanEntry> plan = {Silence(), Voiced(), Silence()};
  EXPECT_EQ(ToFrames(plan, std::vector<int>{0, 10, 0}),
            (std::vector<Frame>{Frame::Voiced(10, 30, kK10Array)}));
}

TEST(FrameEndingsTest, GuardKeepsSilenceBeforeFirstVoicedFrameSounding) {
  const std::vector<Frame> frames = {
      Frame::Unvoiced(5, kK4Array), Frame::Silence(),
      Frame::Voiced(9, 30, kK10Array), Frame::Silence()};
  EXPECT_EQ(
      GuardK510(frames),
      (std::vector<Frame>{Frame::Unvoiced(5, kK4Array), Frame::Repeat(1, 0),
                          Frame::Voiced(9, 30, kK10Array), Frame::Silence()}));
}

TEST(FrameEndingsTest, WithoutFadesOnlyGuardAndStopAreAdded) {
  const std::vector<Frame> frames = {Frame::Voiced(9, 30, kK10Array),
                                     Frame::Unvoiced(10, kK4Array)};
  std::vector<Frame> expected = frames;
  expected.push_back(Frame::Stop());
  EXPECT_EQ(AddFades(frames, false), expected);
}

TEST(FrameEndingsTest, UnvoicedEndFadesInThirdsBeforeTheStop) {
  // Energy index 10 (value 33): 33/3 = 11 is index 7; 11/3 is nearest 4
  // (index 4) in dB; 4/3 is nearest 1 (index 1), which ends the fade.
  const std::vector<Frame> frames = {Frame::Voiced(9, 30, kK10Array),
                                     Frame::Unvoiced(10, kK4Array)};
  EXPECT_EQ(AddFades(frames, true),
            (std::vector<Frame>{Frame::Voiced(9, 30, kK10Array),
                                Frame::Unvoiced(10, kK4Array),
                                Frame::Repeat(7, 0), Frame::Repeat(4, 0),
                                Frame::Repeat(1, 0), Frame::Stop()}));
}

TEST(FrameEndingsTest, FadeReplacesFollowingSilenceOnly) {
  const std::vector<Frame> frames = {
      Frame::Voiced(9, 30, kK10Array), Frame::Unvoiced(10, kK4Array),
      Frame::Silence(), Frame::Voiced(9, 30, kK10Array)};
  EXPECT_EQ(
      AddFades(frames, true),
      (std::vector<Frame>{Frame::Voiced(9, 30, kK10Array),
                          Frame::Unvoiced(10, kK4Array), Frame::Repeat(7, 0),
                          Frame::Voiced(9, 30, kK10Array), Frame::Stop()}));
}

TEST(FrameEndingsTest, QuietUnvoicedEndNeedsNoFade) {
  const std::vector<Frame> frames = {Frame::Voiced(9, 30, kK10Array),
                                     Frame::Unvoiced(2, kK4Array)};
  EXPECT_EQ(AddFades(frames, true),
            (std::vector<Frame>{Frame::Voiced(9, 30, kK10Array),
                                Frame::Unvoiced(2, kK4Array), Frame::Stop()}));
}

// QuietPauses() (speak5220 datasheet §12.2).

const std::array<int, 10> kK = {20, 15, 8, 8, 8, 8, 8, 4, 4, 4};

TEST(QuietPausesTest, NeutralFrameHasEveryKAtItsIndexNearestZero) {
  const Frame neutral = NeutralFrame(30);
  EXPECT_EQ(lpc::FrameKind(neutral), lpc::FrameType::kVoiced);
  EXPECT_EQ(neutral.energy, 1);
  EXPECT_EQ(neutral.pitch, 30);
  for (int i = 0; i < lpc::kKCount; ++i) {
    const std::span<const int> table = lpc::KTable(i);
    const int value = std::abs(table[static_cast<std::size_t>(
        neutral.k.at(static_cast<std::size_t>(i)))]);
    for (const int other : table) {
      EXPECT_LE(value, std::abs(other)) << "K" << i + 1;
    }
  }
}

TEST(QuietPausesTest, SecondFrameOfAPauseOfThreeOrMoreIsNeutral) {
  const Frame v = Frame::Voiced(9, 30, kK);
  const Frame s = Frame::Silence();
  EXPECT_EQ(
      QuietPauses(std::vector<Frame>{v, s, s, s, s, v, Frame::Stop()}),
      (std::vector<Frame>{v, s, NeutralFrame(30), s, s, v, Frame::Stop()}));
  EXPECT_EQ(QuietPauses(std::vector<Frame>{v, s, s, s, v, Frame::Stop()}),
            (std::vector<Frame>{v, s, NeutralFrame(30), s, v, Frame::Stop()}));
}

TEST(QuietPausesTest, PausesOfOneOrTwoFramesAreLeftAsTheyAre) {
  const Frame v = Frame::Voiced(9, 30, kK);
  const Frame s = Frame::Silence();
  for (const std::vector<Frame>& frames :
       {std::vector<Frame>{v, s, v, Frame::Stop()},
        std::vector<Frame>{v, s, s, v, Frame::Stop()}}) {
    EXPECT_EQ(QuietPauses(frames), frames);
  }
}

TEST(QuietPausesTest, PauseBeforeTheStopIsQuietenedToo) {
  const Frame v = Frame::Voiced(9, 30, kK);
  const Frame s = Frame::Silence();
  EXPECT_EQ(QuietPauses(std::vector<Frame>{v, s, s, s, Frame::Stop()}),
            (std::vector<Frame>{v, s, NeutralFrame(30), s, Frame::Stop()}));
}

TEST(QuietPausesTest, NeutralFrameTakesTheLastVoicedPitch) {
  const Frame v = Frame::Voiced(9, 30, kK);
  const Frame u = Frame::Unvoiced(5, {1, 2, 3, 4});
  const Frame s = Frame::Silence();
  EXPECT_EQ(QuietPauses(std::vector<Frame>{v, u, s, s, s, Frame::Stop()})[3],
            NeutralFrame(30));
  // With no voiced frame before, pitch 1.
  EXPECT_EQ(QuietPauses(std::vector<Frame>{u, s, s, s, Frame::Stop()})[2],
            NeutralFrame(1));
}

TEST(QuietPausesTest, RepeatAfterAQuietenedPauseIsSentInFull) {
  const Frame v = Frame::Voiced(9, 30, kK);
  const Frame s = Frame::Silence();
  EXPECT_EQ(QuietPauses(std::vector<Frame>{v, s, s, s, Frame::Repeat(7, 32),
                                           Frame::Stop()}),
            (std::vector<Frame>{v, s, NeutralFrame(30), s,
                                Frame::Voiced(7, 32, kK), Frame::Stop()}));
  // An unvoiced repeat holds K1-K4 and zeroes K5-K10: an unvoiced frame.
  EXPECT_EQ(QuietPauses(std::vector<Frame>{v, s, s, s, Frame::Repeat(7, 0),
                                           Frame::Stop()})[4],
            Frame::Unvoiced(7, {20, 15, 8, 8}));
  const Frame u = Frame::Unvoiced(5, {1, 2, 3, 4});
  EXPECT_EQ(QuietPauses(std::vector<Frame>{u, s, s, s, Frame::Repeat(4, 0),
                                           Frame::Stop()})[4],
            Frame::Unvoiced(4, {1, 2, 3, 4}));
}

TEST(QuietPausesTest, VoicedRepeatAfterAnUnvoicedFrameKeepsItsPause) {
  // It relies on the unvoiced frame's zeroing of K5-K10, which no full
  // frame can give.
  const Frame s = Frame::Silence();
  const std::vector<Frame> frames = {Frame::Unvoiced(5, {1, 2, 3, 4}),
                                     s,
                                     s,
                                     s,
                                     Frame::Repeat(7, 30),
                                     Frame::Stop()};
  EXPECT_EQ(QuietPauses(frames), frames);
}

}  // namespace
}  // namespace speak5220::core::encoder
