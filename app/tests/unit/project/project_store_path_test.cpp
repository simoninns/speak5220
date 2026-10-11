/*
 * File:        project_store_path_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the rule for paths inside a project
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <string>

#include "core/project/project_store.h"
#include "gtest/gtest.h"

namespace speak5220::core::project {
namespace {

TEST(ProjectStorePathTest, AcceptsRelativePathsInsideTheProject) {
  for (const char* path :
       {"project.yaml",
        "phrases/6f2c0000-0000-0000-0000-000000000001/source.wav",
        "notes/read me.txt", "a/b/c/d.e", ".hidden"}) {
    EXPECT_TRUE(IsSafeProjectPath(path)) << path;
  }
}

TEST(ProjectStorePathTest, RefusesPathsThatCouldLeaveTheProject) {
  for (const char* path :
       {"", "/etc/passwd", "../evil", "a/../../evil", "a/./b", "a//b", "a/",
        "C:/windows", "c:evil", "a\\b", "..", ".", "phrases/../x",
        "file\x01name", "caf\xc3\xa9"}) {
    EXPECT_FALSE(IsSafeProjectPath(path)) << path;
  }
}

TEST(ProjectStorePathTest, RefusesVeryLongPaths) {
  EXPECT_TRUE(IsSafeProjectPath(std::string(255, 'a')));
  EXPECT_FALSE(IsSafeProjectPath(std::string(256, 'a')));
}

}  // namespace
}  // namespace speak5220::core::project
