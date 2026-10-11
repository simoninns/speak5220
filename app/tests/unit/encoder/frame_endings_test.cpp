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
#include <vector>

#include "core/encoder/encoder_types.h"
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

}  // namespace
}  // namespace speak5220::core::encoder
