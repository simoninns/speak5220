/*
 * File:        version_test.cpp
 * Module:      util tests
 * Purpose:     Unit tests for the application version
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/version.h"

#include <string_view>

#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

// True if `v` is three non-empty runs of digits separated by dots. Checked
// by hand: Google Test's regular expressions on Windows have no character
// classes, and std::regex trips GCC's -Wmaybe-uninitialized under the
// sanitizers.
bool IsMajorMinorPatch(std::string_view v) {
  int parts = 1;
  bool digit_seen = false;
  for (const char c : v) {
    if (c == '.') {
      if (!digit_seen) {
        return false;
      }
      ++parts;
      digit_seen = false;
    } else if (c >= '0' && c <= '9') {
      digit_seen = true;
    } else {
      return false;
    }
  }
  return parts == 3 && digit_seen;
}

TEST(VersionTest, ReportsMajorMinorPatch) {
  EXPECT_TRUE(IsMajorMinorPatch(Version())) << Version();
}

TEST(VersionTest, VersionCheckRejectsOtherForms) {
  EXPECT_FALSE(IsMajorMinorPatch("1.2"));
  EXPECT_FALSE(IsMajorMinorPatch("1.2.3.4"));
  EXPECT_FALSE(IsMajorMinorPatch("1..3"));
  EXPECT_FALSE(IsMajorMinorPatch("1.2.x"));
  EXPECT_FALSE(IsMajorMinorPatch("1.2."));
  EXPECT_TRUE(IsMajorMinorPatch("10.20.30"));
}

}  // namespace
}  // namespace speak5220::core
