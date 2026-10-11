/*
 * File:        task_runner_test.cpp
 * Module:      util tests
 * Purpose:     Functional tests of the thread-pool task runner and the
 *              file digest, which use real threads and files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/task_runner.h"

#include <QTemporaryDir>
#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

#include "core/util/file_digest.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

TEST(ThreadPoolTaskRunnerTest, WaitReturnsWhenEveryJobHasRun) {
  ThreadPoolTaskRunner runner(4);
  std::atomic<int> ran = 0;
  constexpr int kJobs = 100;
  for (int i = 0; i < kJobs; ++i) {
    runner.Post([&ran] { ran.fetch_add(1); });
  }
  runner.Wait();
  EXPECT_EQ(ran.load(), kJobs);
}

TEST(FileSha256DigestTest, DigestsAFilesBytes) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const std::filesystem::path path =
      std::filesystem::path(directory.path().toStdString()) / "abc.txt";
  std::ofstream(path, std::ios::binary) << "abc";
  const StatusOr<std::string> digest = FileSha256Digest().Sha256(path);
  ASSERT_TRUE(digest.IsOk());
  // FIPS 180-2, Appendix B.1.
  EXPECT_EQ(*digest,
            "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(FileSha256DigestTest, MissingFileIsAnError) {
  EXPECT_EQ(FileSha256Digest().Sha256("/no/such/file").GetStatus().Code(),
            StatusCode::kNotFound);
}

}  // namespace
}  // namespace speak5220::core
