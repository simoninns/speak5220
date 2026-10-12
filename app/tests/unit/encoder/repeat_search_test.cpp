/*
 * File:        repeat_search_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the plan of silence, full frames and repeats
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/repeat_search.h"

#include <algorithm>
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

TEST(RepeatSearchTest, ARunHoldsAtMostTheLongestRunOfRepeats) {
  RepeatSearch search;
  const std::vector<FrameFeatures> feats(kMaxRepeatRun + 2, Frame(true, 0.9));
  const std::vector<PlanKind> kinds = Kinds(search.Plan(feats, {}));
  EXPECT_EQ(std::ranges::count(kinds, kVoiced), 2);
  int run = 0;
  for (const PlanKind kind : kinds) {
    run = kind == kRepeat ? run + 1 : 0;
    EXPECT_LT(run, kMaxRepeatRun);
  }
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

// K indices of a strongly resonant voiced filter (K1 near -1, K2 near
// 1: about 22 dB of gain) and of a mild one (under 3 dB).
const std::vector<int> kResonantK = {2, 28, 8, 6, 2, 6, 8, 2, 4, 3};
const std::vector<int> kMildK = {23, 14, 5, 3, 6, 7, 7, 4, 4, 4};

PlanEntry Voiced(const std::vector<int>& k, int pitch = 20) {
  return {.kind = kVoiced, .k = k, .pitch = pitch};
}

TEST(HoldResonantFiltersTest, FadingOutOfAResonantFilterKeepsItAFrame) {
  ASSERT_GT(FilterGainDb(kResonantK) - FilterGainDb(kMildK), 14.0);
  std::vector<PlanEntry> plan = {Voiced(kResonantK), Voiced(kMildK, 22)};
  HoldResonantFilters(plan, std::vector<double>{0.2, 0.05}, 14.0);
  EXPECT_EQ(plan[1],
            (PlanEntry{.kind = kRepeat, .k = kResonantK, .pitch = 22}));
}

TEST(HoldResonantFiltersTest, ARepeatOfTheResonantFilterIsHeldFromToo) {
  std::vector<PlanEntry> plan = {
      Voiced(kResonantK),
      {.kind = kRepeat, .k = kResonantK, .pitch = 20},
      Voiced(kMildK)};
  HoldResonantFilters(plan, std::vector<double>{0.2, 0.2, 0.05}, 14.0);
  EXPECT_EQ(plan[2].kind, kRepeat);
  EXPECT_EQ(plan[2].k, kResonantK);
}

TEST(HoldResonantFiltersTest, LeavesAChangeThatIsNotAFadeOrNotADrop) {
  const std::vector<PlanEntry> start = {Voiced(kResonantK), Voiced(kMildK)};
  std::vector<PlanEntry> plan = start;
  // The level rises.
  HoldResonantFilters(plan, std::vector<double>{0.05, 0.2}, 14.0);
  EXPECT_EQ(plan, start);
  // The gain drop is under the threshold.
  HoldResonantFilters(plan, std::vector<double>{0.2, 0.05}, 30.0);
  EXPECT_EQ(plan, start);
  // The gain rises.
  std::vector<PlanEntry> rising = {Voiced(kMildK), Voiced(kResonantK)};
  HoldResonantFilters(rising, std::vector<double>{0.2, 0.05}, 14.0);
  EXPECT_EQ(rising[1].kind, kVoiced);
}

TEST(HoldResonantFiltersTest, LeavesChangesTheChipDoesNotInterpolate) {
  // From unvoiced or silence, the chip loads the new K at once.
  const std::vector<int> unvoiced = {31, 31, 15, 15};
  std::vector<PlanEntry> plan = {{.kind = kUnvoiced, .k = unvoiced, .pitch = 0},
                                 Voiced(kMildK),
                                 {},
                                 Voiced(kMildK)};
  const std::vector<PlanEntry> start = plan;
  HoldResonantFilters(plan, std::vector<double>{0.2, 0.05, 0.0, 0.01}, 3.0);
  EXPECT_EQ(plan, start);
}

TEST(HoldResonantFiltersTest, AHeldFrameIsNotHeldAgain) {
  std::vector<PlanEntry> plan = {Voiced(kResonantK), Voiced(kMildK),
                                 Voiced(kMildK)};
  HoldResonantFilters(plan, std::vector<double>{0.4, 0.2, 0.1}, 14.0);
  EXPECT_EQ(plan[1].kind, kRepeat);
  EXPECT_EQ(plan[2], Voiced(kMildK));
}

}  // namespace
}  // namespace speak5220::core::encoder
