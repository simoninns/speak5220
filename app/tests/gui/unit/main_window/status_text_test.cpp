/*
 * File:        status_text_test.cpp
 * Module:      main_window tests
 * Purpose:     Unit tests for the status bar's project size text
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/status_text.h"

#include <QString>
#include <vector>

#include "core/project/phrom_allocator.h"
#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

using core::project::AllocatePhroms;
using core::project::PhraseCostInput;
using core::project::PhromAllocation;
using core::project::PhromFill;

// An allocation of PHROMs with these used bytes (overheads included).
PhromAllocation WithPhroms(const std::vector<int>& used_bytes) {
  PhromAllocation allocation;
  for (const int used : used_bytes) {
    allocation.phroms.push_back(PhromFill{.phrases = {}, .used_bytes = used});
  }
  return allocation;
}

TEST(ProjectSizeTextTest, EmptyProjectUsesNoPhroms) {
  EXPECT_EQ(ProjectSizeText(PhromAllocation{}),
            QStringLiteral("No PHROMs in use"));
}

TEST(ProjectSizeTextTest, MatchesThePlansExample) {
  // Plan §5: two full PHROMs and 12 296 bytes of a third, 4 088 free.
  const PhromAllocation allocation = WithPhroms({16384, 16384, 12296});
  EXPECT_EQ(ProjectSizeText(allocation),
            QStringLiteral("44.0 KB · 3 PHROMs of 64 · 4.0 KB free "
                           "in PHROM 3"));
}

TEST(ProjectSizeTextTest, RoundsToOneDecimal) {
  // 38093 bytes = 37.2 KB; 4198 bytes free = 4.1 KB.
  const PhromAllocation allocation = WithPhroms({16384, 9523, 12186});
  EXPECT_EQ(ProjectSizeText(allocation),
            QStringLiteral("37.2 KB · 3 PHROMs of 64 · 4.1 KB free "
                           "in PHROM 3"));
}

TEST(ProjectSizeTextTest, OnePhromIsSingular) {
  const std::vector<PhraseCostInput> costs = {{.cost = 1000}};
  EXPECT_EQ(ProjectSizeText(AllocatePhroms(costs)),
            QStringLiteral("1.2 KB · 1 PHROM of 64 · 14.8 KB free "
                           "in PHROM 1"));
}

TEST(ProjectSizeTextTest, SaysWhenOverBudget) {
  const PhromAllocation allocation = WithPhroms(std::vector<int>(65, 16384));
  EXPECT_EQ(ProjectSizeText(allocation),
            QStringLiteral("1040.0 KB · 65 PHROMs of 64 · 0.0 KB "
                           "free in PHROM 65 · over budget"));
}

TEST(ProjectSizeTextTest, CountsPhrasesTooLargeOrNotEncoded) {
  const std::vector<PhraseCostInput> costs = {
      {.cost = 20000}, {.cost = std::nullopt}, {.cost = std::nullopt}};
  EXPECT_EQ(ProjectSizeText(AllocatePhroms(costs)),
            QStringLiteral("No PHROMs in use · 1 phrase too large for a "
                           "PHROM · 2 phrases not encoded"));
}

}  // namespace
}  // namespace speak5220::app
