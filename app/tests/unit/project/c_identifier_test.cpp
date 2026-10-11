/*
 * File:        c_identifier_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the C identifiers derived from phrase names
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/c_identifier.h"

#include <string>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::project {
namespace {

TEST(CIdentifierTest, FollowsThePlansExamples) {
  EXPECT_EQ(CIdentifierFromName("ONE,WON"), "ONE_WON");
  EXPECT_EQ(CIdentifierFromName("-ING"), "ING");
  EXPECT_EQ(CIdentifierFromName("2- (TWEN-)"), "W_2_TWEN");
}

TEST(CIdentifierTest, CollapsesRunsOfOtherCharacters) {
  EXPECT_EQ(CIdentifierFromName("O'CLOCK"), "O_CLOCK");
  EXPECT_EQ(CIdentifierFromName("A , - B"), "A_B");
  EXPECT_EQ(CIdentifierFromName("(TONE 1)"), "TONE_1");
}

TEST(CIdentifierTest, PrefixesALeadingDigit) {
  EXPECT_EQ(CIdentifierFromName("-0.125"), "W_0_125");
  EXPECT_EQ(CIdentifierFromName("000"), "W_000");
}

TEST(CIdentifierTest, NameWithNoLetterOrDigitGetsAPlaceholder) {
  EXPECT_EQ(CIdentifierFromName("..."), "W");
  EXPECT_EQ(CIdentifierFromName(""), "W");
}

TEST(CIdentifierTest, ClashesGetANumericSuffix) {
  const std::vector<std::string> names = {"ONE,WON", "ONE WON", "ONE-WON",
                                          "TWO"};
  EXPECT_EQ(
      CIdentifiersForNames(names),
      (std::vector<std::string>{"ONE_WON", "ONE_WON_2", "ONE_WON_3", "TWO"}));
}

TEST(CIdentifierTest, SuffixSkipsIdentifiersAlreadyTaken) {
  const std::vector<std::string> names = {"A 2", "A", "A."};
  EXPECT_EQ(CIdentifiersForNames(names),
            (std::vector<std::string>{"A_2", "A", "A_3"}));
}

}  // namespace
}  // namespace speak5220::core::project
