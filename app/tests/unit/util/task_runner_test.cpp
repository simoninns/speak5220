/*
 * File:        task_runner_test.cpp
 * Module:      util tests
 * Purpose:     Unit tests of the inline task runner
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/task_runner.h"

#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

TEST(InlineTaskRunnerTest, RunsEachJobInsidePostInOrder) {
  InlineTaskRunner runner;
  std::vector<int> ran;
  runner.Post([&ran] { ran.push_back(1); });
  EXPECT_EQ(ran, std::vector<int>{1});
  runner.Post([&ran] { ran.push_back(2); });
  runner.Wait();
  EXPECT_EQ(ran, (std::vector<int>{1, 2}));
}

}  // namespace
}  // namespace speak5220::core
