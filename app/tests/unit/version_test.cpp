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

// True if `v` is seven lower-case hexadecimal digits, optionally followed
// by "-dirty", or is "unknown". Checked by hand: Google Test's regular
// expressions on Windows have no character classes, and std::regex trips
// GCC's -Wmaybe-uninitialized under the sanitizers.
bool IsGitVersion(std::string_view v) {
  if (v == "unknown") {
    return true;
  }
  constexpr std::string_view kDirty = "-dirty";
  if (v.ends_with(kDirty)) {
    v.remove_suffix(kDirty.size());
  }
  if (v.size() != 7) {
    return false;
  }
  for (const char c : v) {
    if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) {
      return false;
    }
  }
  return true;
}

TEST(VersionTest, ReportsGitHash) {
  EXPECT_TRUE(IsGitVersion(Version())) << Version();
}

TEST(VersionTest, VersionCheckRejectsOtherForms) {
  EXPECT_TRUE(IsGitVersion("1ec2afb"));
  EXPECT_TRUE(IsGitVersion("1ec2afb-dirty"));
  EXPECT_TRUE(IsGitVersion("unknown"));
  EXPECT_FALSE(IsGitVersion("0.1.0"));
  EXPECT_FALSE(IsGitVersion("1ec2af"));
  EXPECT_FALSE(IsGitVersion("1ec2afb0"));
  EXPECT_FALSE(IsGitVersion("1EC2AFB"));
  EXPECT_FALSE(IsGitVersion("1ec2afb-"));
  EXPECT_FALSE(IsGitVersion("-dirty"));
}

}  // namespace
}  // namespace speak5220::core
