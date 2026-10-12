/*
 * File:        voicing_classifier_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the per-frame voicing decision and the octave
 *              fix
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/voicing_classifier.h"

#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "core/encoder/encoder_types.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

PitchTrack TrackOf(const std::vector<double>& periods) {
  PitchTrack t;
  for (std::size_t h = 0; h < periods.size(); ++h) {
    t.hops.push_back(static_cast<int>(h) * 40);
  }
  t.periods = periods;
  return t;
}

TEST(VoicingClassifierTest, VoicedWhenMostOfTheThreeNearestHopsAre) {
  VoicingClassifier c;
  // Frame at sample 80: hops 1, 2, 3.
  const std::vector<int> centre = {80};
  EXPECT_TRUE(c.Classify(TrackOf({0, 50, 0, 52, 0}), centre)[0].voiced);
  EXPECT_FALSE(c.Classify(TrackOf({0, 50, 0, 0, 0}), centre)[0].voiced);
}

TEST(VoicingClassifierTest, PeriodIsMedianOfVoicedHops) {
  VoicingClassifier c;
  const auto v = c.Classify(TrackOf({0, 50, 60, 55, 0}), std::vector<int>{80});
  EXPECT_EQ(v[0].period, 55.0);
  const auto two = c.Classify(TrackOf({0, 50, 0, 60, 0}), std::vector<int>{80});
  EXPECT_EQ(two[0].period, 55.0);
}

TEST(VoicingClassifierTest, EdgeFramesCountOnlyHopsThatExist) {
  VoicingClassifier c;
  // Frame at sample 0: hops 0 and 1 only; one voiced of two is no
  // majority.
  EXPECT_FALSE(c.Classify(TrackOf({50, 0, 0}), std::vector<int>{0})[0].voiced);
  EXPECT_TRUE(c.Classify(TrackOf({50, 52, 0}), std::vector<int>{0})[0].voiced);
}

TEST(VoicingClassifierTest, NearestHopRoundsHalvesToEven) {
  VoicingClassifier c;
  // Sample 20 is half-way between hops 0 and 1: Python's round() gives
  // hop 0, so the hops counted are 0 and 1.
  EXPECT_TRUE(
      c.Classify(TrackOf({50, 52, 0, 0}), std::vector<int>{20})[0].voiced);
  // Sample 60 is half-way between hops 1 and 2: round() gives hop 2, so
  // the hops counted are 1, 2 and 3.
  EXPECT_FALSE(
      c.Classify(TrackOf({50, 52, 0, 0}), std::vector<int>{60})[0].voiced);
}

TEST(VoicingClassifierTest, HalvesAPeriodTwiceItsNeighbours) {
  VoicingClassifier c;
  std::vector<double> periods(30, 50.0);
  for (std::size_t h = 9; h <= 11; ++h) {
    periods[h] = 100.0;  // frame 2 (centre 400) sees hops 9-11
  }
  const std::vector<int> centres = {0, 200, 400, 600, 800};
  const auto v = c.Classify(TrackOf(periods), centres);
  EXPECT_EQ(v[2].period, 50.0);
}

TEST(VoicingClassifierTest, DoublesAPeriodHalfItsNeighbours) {
  VoicingClassifier c;
  std::vector<double> periods(30, 80.0);
  for (std::size_t h = 9; h <= 11; ++h) {
    periods[h] = 40.0;
  }
  const std::vector<int> centres = {0, 200, 400, 600, 800};
  const auto v = c.Classify(TrackOf(periods), centres);
  EXPECT_EQ(v[2].period, 80.0);
}

TEST(VoicingClassifierTest, OctaveFixStaysWithinPitchRange) {
  VoicingClassifier c;
  std::vector<double> periods(30, 100.0);
  for (std::size_t h = 9; h <= 11; ++h) {
    periods[h] = 50.0;  // doubling would pass 100, but 100 <= 160: fixed
  }
  std::vector<double> high(30, 150.0);
  for (std::size_t h = 9; h <= 11; ++h) {
    high[h] = 85.0;  // doubling would give 170 > 160: left alone
  }
  const std::vector<int> centres = {0, 200, 400, 600, 800};
  EXPECT_EQ(c.Classify(TrackOf(periods), centres)[2].period, 100.0);
  EXPECT_EQ(c.Classify(TrackOf(high), centres)[2].period, 85.0);
}

// The voicing of each frame, as a string: V voiced, U unvoiced.
std::vector<FrameVoicing> Voicing(std::string_view kinds) {
  std::vector<FrameVoicing> v;
  for (const char c : kinds) {
    v.push_back({.voiced = c == 'V', .period = c == 'V' ? 80.0 : 0.0});
  }
  return v;
}

std::string Kinds(const std::vector<FrameVoicing>& v) {
  std::string s;
  for (const FrameVoicing& f : v) {
    s += f.voiced ? 'V' : 'U';
  }
  return s;
}

TEST(SmoothVoicingRunsTest, FillsShortUnvoicedIslandsWithTheMeanPeriod) {
  std::vector<FrameVoicing> v = Voicing("VVVUUVVV");
  v[2].period = 70.0;
  v[5].period = 90.0;
  SmoothVoicingRuns(v, 3);
  EXPECT_EQ(Kinds(v), "VVVVVVVV");
  EXPECT_EQ(v[3].period, 80.0);
  EXPECT_EQ(v[4].period, 80.0);
}

TEST(SmoothVoicingRunsTest, MakesShortVoicedIslandsUnvoiced) {
  std::vector<FrameVoicing> v = Voicing("UUUVVUUU");
  SmoothVoicingRuns(v, 3);
  EXPECT_EQ(Kinds(v), "UUUUUUUU");
  EXPECT_EQ(v[3].period, 0.0);
}

TEST(SmoothVoicingRunsTest, KeepsRunsAsLongAsTheMinimumAndRunsAtTheEnds) {
  std::vector<FrameVoicing> v = Voicing("UVVVUUUVU");
  SmoothVoicingRuns(v, 3);
  // The voiced run of 3 stays; the lone V before the last U is an island,
  // but the U after it is a run at the end, so it stays too.
  EXPECT_EQ(Kinds(v), "UVVVUUUUU");
}

TEST(SmoothVoicingRunsTest, UnvoicedIslandsGoFirst) {
  // The lone U is filled first, so the voiced run around it is long enough
  // to stay.
  std::vector<FrameVoicing> v = Voicing("UUUVUVUUU");
  SmoothVoicingRuns(v, 3);
  EXPECT_EQ(Kinds(v), "UUUVVVUUU");
}

TEST(SmoothVoicingRunsTest, MinimumOfOneChangesNothing) {
  std::vector<FrameVoicing> v = Voicing("VUVUV");
  SmoothVoicingRuns(v, 1);
  EXPECT_EQ(Kinds(v), "VUVUV");
}

}  // namespace
}  // namespace speak5220::core::encoder
