/*
 * File:        cli_version_test.cc
 * Module:      cli tests
 * Purpose:     Runs the speak5220-cli executable and checks the version it
 * prints
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QProcess>
#include <QString>
#include <QStringList>

#include "core/util/version.h"
#include "gtest/gtest.h"

namespace speak5220::cli {
namespace {

// Time allowed for the executable to start and exit.
constexpr int kProcessTimeoutMs = 30000;

QString RunCli(const QStringList& arguments) {
  QProcess process;
  process.start(QStringLiteral(SPEAK5220_CLI_PATH), arguments);
  EXPECT_TRUE(process.waitForFinished(kProcessTimeoutMs))
      << "speak5220-cli did not finish: " << SPEAK5220_CLI_PATH;
  EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
  EXPECT_EQ(process.exitCode(), 0);
  return QString::fromUtf8(process.readAllStandardOutput()).trimmed();
}

QString ExpectedVersionLine() {
  const std::string_view version = core::Version();
  return QStringLiteral("speak5220-cli ") +
         QString::fromUtf8(version.data(),
                           static_cast<qsizetype>(version.size()));
}

TEST(CliVersionTest, PrintsVersionWhenRunWithoutArguments) {
  EXPECT_EQ(RunCli({}), ExpectedVersionLine());
}

TEST(CliVersionTest, PrintsVersionForVersionOption) {
  EXPECT_EQ(RunCli({QStringLiteral("--version")}), ExpectedVersionLine());
}

}  // namespace
}  // namespace speak5220::cli
