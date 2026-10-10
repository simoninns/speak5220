/*
 * File:        repeat_search_test.cc
 * Module:      encoder tests
 * Purpose:     Unit tests of the plan of silence, full frames and repeats
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/repeat_search.h"

#include <cstddef>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/quantiser.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

// A sounding frame with the autocorrelation of an AR(1) process with
// coefficient `a` (r[i] = a^i).
FrameFeatures Frame(bool voiced, double a, double period = 80.0) {
  FrameFeatures f;
  f.level = 0.1;
  f.voiced = voiced;
  f.period = voiced ? period : 0.0;
  double v = 1.0;
  for (std::size_t i = 0; i < kAutocorrelationSize; ++i) {
    f.rv.at(i) = v;
    f.ru.at(i) = v;
    v *= a;
  }
  f.rv[0] *= 1.0001;
  f.ru[0] *= 1.0001;
  return f;
}

FrameFeatures Gated() {
  FrameFeatures f = Frame(false, 0.5);
  f.gated = true;
  return f;
}

std::vector<PlanKind> Kinds(const std::vector<PlanEntry>& plan) {
  std::vector<PlanKind> out;
  out.reserve(plan.size());
  for (const PlanEntry& p : plan) {
    out.push_back(p.kind);
  }
  return out;
}

using enum PlanKind;

TEST(RepeatSearchTest, GatedFramesAreSilenceAndTrailingSilenceIsDropped) {
  RepeatSearch search;
  const std::vector<FrameFeatures> feats = {Gated(), Frame(true, 0.9), Gated(),
                                            Gated()};
  EXPECT_EQ(Kinds(search.Plan(feats, {})),
            (std::vector<PlanKind>{kSilence, kVoiced}));
}

TEST(RepeatSearchTest, SteadySoundIsOneFullFrameAndRepeats) {
  RepeatSearch search;
  const std::vector<FrameFeatures> feats(4, Frame(true, 0.9));
  const std::vector<PlanEntry> plan = search.Plan(feats, {});
  EXPECT_EQ(Kinds(plan),
            (std::vector<PlanKind>{kVoiced, kRepeat, kRepeat, kRepeat}));
  for (const PlanEntry& p : plan) {
    EXPECT_EQ(p.k, plan.front().k);
    EXPECT_EQ(p.pitch, PitchIndex(80.0));
    EXPECT_EQ(p.k.size(), 10U);
  }
}

TEST(RepeatSearchTest, RunsAreAtMostEightFrames) {
  RepeatSearch search;
  const std::vector<FrameFeatures> feats(10, Frame(false, 0.6));
  const std::vector<PlanEntry> plan = search.Plan(feats, {});
  ASSERT_EQ(plan.size(), 10U);
  int full = 0;
  for (const PlanEntry& p : plan) {
    full += p.kind == kUnvoiced ? 1 : 0;
    EXPECT_EQ(p.k.size(), 4U);
    EXPECT_EQ(p.pitch, 0);
  }
  EXPECT_EQ(full, 2);
}

TEST(RepeatSearchTest, WithoutRepeatsEveryFrameIsFull) {
  RepeatSearch search;
  EncoderOptions options;
  options.repeats = false;
  const std::vector<FrameFeatures> feats(3, Frame(true, 0.9));
  EXPECT_EQ(Kinds(search.Plan(feats, options)),
            (std::vector<PlanKind>{kVoiced, kVoiced, kVoiced}));
}

TEST(RepeatSearchTest, VoicingChangeStartsANewFullFrame) {
  RepeatSearch search;
  const std::vector<FrameFeatures> feats = {
      Frame(true, 0.9), Frame(true, 0.9), Frame(false, 0.9), Frame(false, 0.9)};
  EXPECT_EQ(Kinds(search.Plan(feats, {})),
            (std::vector<PlanKind>{kVoiced, kRepeat, kUnvoiced, kRepeat}));
}

TEST(RepeatSearchTest, DifferentSpectraAreSentInFullWhenDistortionCostsMore) {
  RepeatSearch search;
  EncoderOptions options;
  options.repeat_lambda = 1000.0;  // distortion is very expensive
  const std::vector<FrameFeatures> feats = {Frame(true, 0.9),
                                            Frame(true, -0.9)};
  EXPECT_EQ(Kinds(search.Plan(feats, options)),
            (std::vector<PlanKind>{kVoiced, kVoiced}));
  options.repeat_lambda = 0.0;  // only bits count
  EXPECT_EQ(Kinds(search.Plan(feats, options)),
            (std::vector<PlanKind>{kVoiced, kRepeat}));
}

TEST(RepeatSearchTest, PitchIsClampedToTheTableRange) {
  RepeatSearch search;
  const std::vector<FrameFeatures> feats = {Frame(true, 0.9, 200.0)};
  EXPECT_EQ(search.Plan(feats, {})[0].pitch, PitchIndex(160.0));
}

}  // namespace
}  // namespace speak5220::core::encoder
