/*
 * File:        version_test.cc
 * Module:      util tests
 * Purpose:     Unit tests for the application version
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/version.h"

#include <regex>
#include <string>

#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

// std::regex rather than gMock's MatchesRegex: on Windows Google Test uses
// its own simple regular expressions, which have no character classes.
TEST(VersionTest, ReportsMajorMinorPatch) {
  const std::regex pattern(R"([0-9]+\.[0-9]+\.[0-9]+)");
  EXPECT_TRUE(std::regex_match(std::string(Version()), pattern)) << Version();
}

}  // namespace
}  // namespace speak5220::core
