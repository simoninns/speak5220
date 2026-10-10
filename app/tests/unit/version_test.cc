/*
 * File:        version_test.cc
 * Module:      util tests
 * Purpose:     Unit tests for the application version
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/version.h"

#include <string>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

using ::testing::MatchesRegex;

TEST(VersionTest, ReportsMajorMinorPatch) {
  EXPECT_THAT(std::string(Version()),
              MatchesRegex(R"([0-9]+\.[0-9]+\.[0-9]+)"));
}

}  // namespace
}  // namespace speak5220::core
