/*
 * File:        frame_track_comparison_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the frame track comparison on hand-made
 *              frame pairs: voicing, onsets and offsets, energy, shape,
 *              pitch, repeats and fade-outs
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/frame_track_comparison.h"

#include <cstddef>
#include <vector>

#include "core/lpc/frame.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::quality {
namespace {

using lpc::Frame;
using ::testing::Each;
using ::testing::ElementsAre;
using testing::kNeutralUnvoicedK;
using testing::kNeutralVoicedK;

// Pitch indices 1 and 17 hold 15 and 31: periods of 16 and 32 samples
// (speak5220 datasheet §8.1, Table 8-5).
constexpr int kPitch16 = 1;
constexpr int kPitch32 = 17;

Frame V(int energy = 10, int pitch = kPitch32) {
  return Frame::Voiced(energy, pitch, kNeutralVoicedK);
}
Frame U(int energy = 10) { return Frame::Unvoiced(energy, kNeutralUnvoicedK); }

// Unvoiced, then a vowel of `voiced` frames, then unvoiced, then a stop.
std::vector<Frame> Word(int voiced = 6) {
  std::vector<Frame> frames = {U(), U()};
  for (int i = 0; i < voiced; ++i) {
    frames.push_back(V());
  }
  frames.push_back(U());
  frames.push_back(U());
  frames.push_back(Frame::Stop());
  return frames;
}

std::size_t Bin(int lag) {
  return static_cast<std::size_t>(lag - kFirstBoundaryLag);
}

TEST(FrameTrackComparisonTest, IdenticalTracksAgreeEverywhere) {
  const std::vector<Frame> word = Word();
  const FrameTrackComparison c = CompareFrameTracks(word, word);
  EXPECT_EQ(c.shift, 0);
  EXPECT_EQ(c.matched, 10);
  EXPECT_EQ(c.voicing_agrees, 10);
  EXPECT_THAT(c.voiced_energy_differences, Each(0.0));
  EXPECT_THAT(c.unvoiced_energy_differences, Each(0.0));
  EXPECT_THAT(c.voiced_shape_errors_db, Each(0.0));
  EXPECT_THAT(c.pitch_differences, Each(0.0));
  EXPECT_EQ(c.reference_onsets, 1);
  EXPECT_EQ(c.onset_lags[Bin(0)], 1);
  EXPECT_EQ(c.reference_offsets, 1);
  EXPECT_EQ(c.offset_lags[Bin(0)], 1);
}

TEST(FrameTrackComparisonTest, ALateOnsetIsALoudOnsetFrameOneFrameLate) {
  const std::vector<Frame> reference = Word();
  std::vector<Frame> encoded = reference;
  encoded[2] = U(kLoudOnsetEnergy + 1);
  const FrameTrackComparison c = CompareFrameTracks(reference, encoded);
  ASSERT_EQ(c.shift, 0);
  EXPECT_EQ(c.voiced_as_unvoiced_onset, 1);
  EXPECT_EQ(c.loud_onset_frames, 1);
  EXPECT_EQ(c.voiced_as_unvoiced_inside, 0);
  EXPECT_EQ(c.onset_lags[Bin(1)], 1);
}

TEST(FrameTrackComparisonTest, ClassifiesVoicingLostInsideAndAtTheOffset) {
  const std::vector<Frame> reference = Word();
  std::vector<Frame> encoded = reference;
  encoded[4] = U();
  encoded[7] = U();
  const FrameTrackComparison c = CompareFrameTracks(reference, encoded);
  ASSERT_EQ(c.shift, 0);
  EXPECT_EQ(c.voiced_as_unvoiced_inside, 1);
  EXPECT_EQ(c.voiced_as_unvoiced_offset, 1);
  EXPECT_EQ(c.offset_lags[Bin(-1)], 1);
  EXPECT_EQ(c.voicing_agrees, 8);
}

TEST(FrameTrackComparisonTest, CountsUnvoicedFramesVoiced) {
  const std::vector<Frame> reference = Word();
  std::vector<Frame> encoded = reference;
  encoded[1] = V();
  const FrameTrackComparison c = CompareFrameTracks(reference, encoded);
  EXPECT_EQ(c.unvoiced_as_voiced, 1);
  EXPECT_EQ(c.onset_lags[Bin(-1)], 1);
}

TEST(FrameTrackComparisonTest, AnOnsetNeverVoicedFallsInTheLastBin) {
  std::vector<Frame> unvoiced = Word();
  for (std::size_t i = 2; i < 8; ++i) {
    unvoiced[i] = U();
  }
  const FrameTrackComparison c = CompareFrameTracks(Word(), unvoiced);
  EXPECT_EQ(c.onset_lags.back(), 1);
}

TEST(FrameTrackComparisonTest, LinesTheTracksUpByTheirLevels) {
  const std::vector<Frame> reference = Word();
  std::vector<Frame> encoded = {Frame::Silence()};
  encoded.insert(encoded.end(), reference.begin(), reference.end());
  const FrameTrackComparison c = CompareFrameTracks(reference, encoded);
  EXPECT_EQ(c.shift, 1);
  EXPECT_EQ(c.voicing_agrees, 10);
}

TEST(FrameTrackComparisonTest, MeasuresEnergyAndPitchDifferences) {
  const std::vector<Frame> reference = {V(10, kPitch32), V(10, kPitch32), U(6),
                                        Frame::Stop()};
  const std::vector<Frame> encoded = {V(11, kPitch16), V(11, kPitch16), U(7),
                                      Frame::Stop()};
  const FrameTrackComparison c = CompareFrameTracks(reference, encoded);
  ASSERT_EQ(c.shift, 0);
  EXPECT_THAT(c.voiced_energy_differences, ElementsAre(1.0, 1.0));
  EXPECT_THAT(c.unvoiced_energy_differences, ElementsAre(1.0));
  EXPECT_THAT(c.pitch_differences, ElementsAre(-12.0, -12.0));
}

TEST(FrameTrackComparisonTest, AShapeErrorIgnoresAGainDifference) {
  std::array<int, lpc::kVoicedKCount> k = kNeutralVoicedK;
  k[0] = 25;
  const std::vector<Frame> reference = {V(), V(), Frame::Stop()};
  const std::vector<Frame> same = {V(12), V(12), Frame::Stop()};
  const std::vector<Frame> other = {Frame::Voiced(10, kPitch32, k),
                                    Frame::Voiced(10, kPitch32, k),
                                    Frame::Stop()};
  EXPECT_THAT(CompareFrameTracks(reference, same).voiced_shape_errors_db,
              Each(0.0));
  EXPECT_THAT(CompareFrameTracks(reference, other).voiced_shape_errors_db,
              Each(::testing::Gt(1.0)));
}

TEST(FrameTrackComparisonTest, RepeatsTakeTheLastFullFramesFilter) {
  const std::vector<Frame> reference = {V(), V(), V(), Frame::Stop()};
  const std::vector<Frame> encoded = {V(), Frame::Repeat(10, kPitch32),
                                      Frame::Repeat(10, kPitch32),
                                      Frame::Stop()};
  const FrameTrackComparison c = CompareFrameTracks(reference, encoded);
  EXPECT_THAT(c.voiced_shape_errors_db, ElementsAre(0.0, 0.0, 0.0));
  EXPECT_EQ(c.encoded.repeats, 2);
  EXPECT_THAT(c.encoded.repeat_runs, ElementsAre(0, 1, 0, 0));
  EXPECT_EQ(c.encoded.longest_repeat_run, 2);
}

TEST(FrameTrackComparisonTest, CountsFadeOutsThatEndTheSound) {
  const std::vector<Frame> faded = {V(),
                                    U(9),
                                    Frame::Repeat(3, 0),
                                    Frame::Repeat(1, 0),
                                    Frame::Silence(),
                                    V(),
                                    U(9),
                                    Frame::Repeat(3, 0),
                                    V(),
                                    U(9),
                                    Frame::Repeat(3, 0),
                                    Frame::Stop()};
  const FrameTrackFacts facts = DescribeFrameTrack(faded);
  EXPECT_EQ(facts.fade_outs, 2);
  EXPECT_EQ(facts.fade_frames, 3);
  EXPECT_EQ(facts.repeats, 4);
  EXPECT_EQ(facts.sounding, 10);
  EXPECT_EQ(facts.frames, 12);
}

TEST(FrameTrackComparisonTest, PoolsComparisons) {
  const std::vector<Frame> reference = Word();
  std::vector<Frame> encoded = reference;
  encoded[4] = U();
  FrameTrackComparison total = CompareFrameTracks(reference, reference);
  AddComparison(total, CompareFrameTracks(reference, encoded));
  EXPECT_EQ(total.matched, 20);
  EXPECT_EQ(total.voicing_agrees, 19);
  EXPECT_EQ(total.voiced_as_unvoiced_inside, 1);
  EXPECT_EQ(total.reference_onsets, 2);
  EXPECT_EQ(total.reference.frames, 22);
}

TEST(FrameTrackComparisonTest, AFilterOfOrderZeroIsFlat) {
  EXPECT_THAT(FilterResponseDb(kNeutralVoicedK, 0), Each(0.0));
}

}  // namespace
}  // namespace speak5220::core::quality
