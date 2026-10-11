/*
 * File:        phrom_allocator_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the list-order PHROM assignment and the
 *              project budget
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrom_allocator.h"

#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::project {
namespace {

std::vector<PhraseCostInput> Costs(const std::vector<int>& costs) {
  std::vector<PhraseCostInput> inputs;
  inputs.reserve(costs.size());
  for (const int cost : costs) {
    inputs.push_back({.cost = cost});
  }
  return inputs;
}

TEST(PhromAllocatorTest, FixedOverheadMatchesTheAcornLayout) {
  // 64-byte header, 95 ASCII pointers, the table's end marker and the null
  // word (speak5220 datasheet §12.8).
  EXPECT_EQ(kPhromFixedOverhead, 256);
  EXPECT_EQ(kPhromWordCapacity, 16128);
  EXPECT_EQ(PhraseCost(100, 5), 107);
}

TEST(PhromAllocatorTest, ProjectThatFillsExactlyOnePhrom) {
  const PhromAllocation allocation = AllocatePhroms(Costs({8000, 8000, 128}));
  ASSERT_EQ(allocation.PhromsUsed(), 1);
  EXPECT_EQ(allocation.phroms[0].used_bytes, 16384);
  EXPECT_EQ(allocation.phroms[0].FreeBytes(), 0);
  EXPECT_EQ(allocation.FreeInLastPhrom(), 0);
  EXPECT_EQ(allocation.TotalBytes(), 16384);
  EXPECT_EQ(allocation.phroms[0].phrases, (std::vector<int>{0, 1, 2}));
}

TEST(PhromAllocatorTest, StartsNewPhromWhenNextPhraseDoesNotFit) {
  const PhromAllocation allocation =
      AllocatePhroms(Costs({8000, 8000, 129, 10}));
  ASSERT_EQ(allocation.PhromsUsed(), 2);
  EXPECT_EQ(allocation.phroms[0].phrases, (std::vector<int>{0, 1}));
  EXPECT_EQ(allocation.phroms[1].phrases, (std::vector<int>{2, 3}));
  EXPECT_EQ(allocation.phroms[1].used_bytes, 256 + 129 + 10);
  EXPECT_EQ(allocation.FreeInLastPhrom(), 16384 - 395);
}

TEST(PhromAllocatorTest, NumbersWordsFrom127InEachPhrom) {
  const PhromAllocation allocation = AllocatePhroms(Costs({9000, 9000, 10}));
  EXPECT_EQ(allocation.phrases[0], (PhraseSlot{.placement = Placement::kPlaced,
                                               .phrom = 1,
                                               .word_number = 127,
                                               .cost = 9000}));
  EXPECT_EQ(allocation.phrases[1].phrom, 2);
  EXPECT_EQ(allocation.phrases[1].word_number, 127);
  EXPECT_EQ(allocation.phrases[2].phrom, 2);
  EXPECT_EQ(allocation.phrases[2].word_number, 128);
}

TEST(PhromAllocatorTest, EarlierPhromsAreNotBackFilled) {
  // The third phrase would fit in PHROM 1, but PHROM 2 has started.
  const PhromAllocation allocation = AllocatePhroms(Costs({10000, 10000, 100}));
  EXPECT_EQ(allocation.phrases[2].phrom, 2);
}

TEST(PhromAllocatorTest, PhraseTooLargeForAnEmptyPhromIsLeftOut) {
  const PhromAllocation allocation =
      AllocatePhroms(Costs({100, kPhromWordCapacity + 1, 200}));
  EXPECT_EQ(allocation.phrases[1].placement, Placement::kTooLarge);
  EXPECT_EQ(allocation.phrases[1].phrom, 0);
  EXPECT_EQ(allocation.phrases[1].cost, kPhromWordCapacity + 1);
  ASSERT_EQ(allocation.PhromsUsed(), 1);
  EXPECT_EQ(allocation.phroms[0].phrases, (std::vector<int>{0, 2}));
  EXPECT_EQ(allocation.phrases[2].word_number, 128);
}

TEST(PhromAllocatorTest, PhraseThatExactlyFillsAnEmptyPhromIsPlaced) {
  const PhromAllocation allocation =
      AllocatePhroms(Costs({kPhromWordCapacity}));
  EXPECT_EQ(allocation.phrases[0].placement, Placement::kPlaced);
  EXPECT_EQ(allocation.FreeInLastPhrom(), 0);
}

TEST(PhromAllocatorTest, UnencodedPhrasesTakeNoSpace) {
  std::vector<PhraseCostInput> inputs = Costs({100, 100});
  inputs.insert(inputs.begin() + 1, PhraseCostInput{});
  const PhromAllocation allocation = AllocatePhroms(inputs);
  EXPECT_EQ(allocation.phrases[1].placement, Placement::kNotEncoded);
  EXPECT_EQ(allocation.phroms[0].phrases, (std::vector<int>{0, 2}));
}

TEST(PhromAllocatorTest, EmptyProjectUsesNoPhrom) {
  const PhromAllocation allocation = AllocatePhroms({});
  EXPECT_EQ(allocation.PhromsUsed(), 0);
  EXPECT_EQ(allocation.TotalBytes(), 0);
  EXPECT_EQ(allocation.FreeInLastPhrom(), 0);
  EXPECT_TRUE(allocation.WithinBudget());
}

TEST(PhromAllocatorTest, SixtyFourFullPhromsAreWithinTheBudget) {
  const std::vector<int> full(kPhromBudget, kPhromWordCapacity);
  const PhromAllocation allocation = AllocatePhroms(Costs(full));
  EXPECT_EQ(allocation.PhromsUsed(), 64);
  EXPECT_TRUE(allocation.WithinBudget());
  EXPECT_EQ(allocation.TotalBytes(), 64 * 16384);
}

TEST(PhromAllocatorTest, AdditionNeedingA65thPhromExceedsTheBudget) {
  std::vector<int> full(kPhromBudget, kPhromWordCapacity);
  full.push_back(1 + 2);
  const PhromAllocation allocation = AllocatePhroms(Costs(full));
  EXPECT_EQ(allocation.PhromsUsed(), 65);
  EXPECT_FALSE(allocation.WithinBudget());
  EXPECT_EQ(allocation.phrases.back().phrom, 65);
}

}  // namespace
}  // namespace speak5220::core::project
