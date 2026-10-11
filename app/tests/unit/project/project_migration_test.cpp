/*
 * File:        project_migration_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the migration of older project.yaml versions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_migration.h"

#include <yaml-cpp/yaml.h>

#include <string>
#include <vector>

#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace speak5220::core::project {
namespace {

using ::testing::HasSubstr;

// Two made-up steps: version 1 renamed "title" to "name", and version 2
// added "added".
std::vector<MigrationStep> TwoSteps(std::vector<int>& ran) {
  return {
      [&ran](YAML::Node& root) {
        ran.push_back(1);
        root["name"] = root["title"].Scalar();
        root.remove("title");
        return OkStatus();
      },
      [&ran](YAML::Node& root) {
        ran.push_back(2);
        root["added"] = "yes";
        return OkStatus();
      },
  };
}

TEST(ProjectMigrationTest, ThisBuildIsTheFirstVersion) {
  EXPECT_EQ(kProjectFormatVersion, 1);
  EXPECT_EQ(static_cast<int>(ProjectMigrationSteps().size()),
            kProjectFormatVersion - 1);
}

TEST(ProjectMigrationTest, RunsEveryStepFromTheDocumentsVersion) {
  std::vector<int> ran;
  const std::vector<MigrationStep> steps = TwoSteps(ran);
  YAML::Node root = YAML::Load("version: 1\ntitle: \"old\"\n");
  ASSERT_TRUE(MigrateProjectDocument(root, 1, steps).IsOk());
  EXPECT_EQ(ran, (std::vector<int>{1, 2}));
  EXPECT_EQ(root["name"].Scalar(), "old");
  EXPECT_FALSE(root["title"].IsDefined());
  EXPECT_EQ(root["added"].Scalar(), "yes");
  EXPECT_EQ(root["version"].Scalar(), "3");
}

TEST(ProjectMigrationTest, StartsFromTheDocumentsVersion) {
  std::vector<int> ran;
  const std::vector<MigrationStep> steps = TwoSteps(ran);
  YAML::Node root = YAML::Load("version: 2\nname: \"n\"\n");
  ASSERT_TRUE(MigrateProjectDocument(root, 2, steps).IsOk());
  EXPECT_EQ(ran, (std::vector<int>{2}));
}

TEST(ProjectMigrationTest, CurrentVersionNeedsNoStep) {
  std::vector<int> ran;
  const std::vector<MigrationStep> steps = TwoSteps(ran);
  YAML::Node root = YAML::Load("version: 3\n");
  ASSERT_TRUE(MigrateProjectDocument(root, 3, steps).IsOk());
  EXPECT_TRUE(ran.empty());
}

TEST(ProjectMigrationTest, RefusesANewerVersion) {
  YAML::Node root = YAML::Load("version: 2\n");
  const Status status = MigrateProjectDocument(root, 2, {});
  EXPECT_EQ(status.Code(), StatusCode::kFailedPrecondition);
  EXPECT_THAT(status.Message(), HasSubstr("newer version of Speak5220"));
}

TEST(ProjectMigrationTest, RefusesAVersionBelowOne) {
  YAML::Node root = YAML::Load("version: 0\n");
  EXPECT_EQ(MigrateProjectDocument(root, 0, {}).Code(),
            StatusCode::kInvalidArgument);
}

TEST(ProjectMigrationTest, FailingStepStopsTheMigration) {
  // Added after construction: GCC 15 at -O3 wrongly warns that a
  // std::function built inside a braced list may be used uninitialised.
  std::vector<MigrationStep> steps;
  steps.emplace_back([](YAML::Node&) { return DataLossError("no title"); });
  YAML::Node root = YAML::Load("version: 1\n");
  const Status status = MigrateProjectDocument(root, 1, steps);
  EXPECT_EQ(status.Code(), StatusCode::kDataLoss);
  EXPECT_THAT(status.Message(), HasSubstr("from format version 1: no title"));
}

}  // namespace
}  // namespace speak5220::core::project
