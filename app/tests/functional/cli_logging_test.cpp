/*
 * File:        cli_logging_test.cpp
 * Module:      cli tests
 * Purpose:     Runs speak5220-cli with the logging options and checks the
 *              console and the log file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace speak5220::cli {
namespace {

using ::testing::Contains;
using ::testing::IsEmpty;
using ::testing::Not;
using ::testing::UnorderedElementsAreArray;

// Time allowed for the executable to start and exit.
constexpr int kProcessTimeoutMs = 30000;

struct CliRun {
  int exit_code = -1;
  QString out;
  QString err;
};

CliRun RunCli(const QStringList& arguments) {
  QProcess process;
  process.start(QStringLiteral(SPEAK5220_CLI_PATH), arguments);
  EXPECT_TRUE(process.waitForFinished(kProcessTimeoutMs));
  return {.exit_code = process.exitCode(),
          .out = QString::fromUtf8(process.readAllStandardOutput()),
          .err = QString::fromUtf8(process.readAllStandardError())};
}

// "level module message" for each log line, from the console form
// "[level] [module] message" or the file form
// "date time [level] [thread N] [module] message".
QStringList Messages(const QString& text) {
  static const QRegularExpression kLine(QStringLiteral(
      R"(^(?:\S+ \S+ )?\[(\w+)\] (?:\[thread \d+\] )?\[(\w+)\] (.*)$)"));
  QStringList out;
  for (const QString& line : text.split('\n', Qt::SkipEmptyParts)) {
    const QRegularExpressionMatch match = kLine.match(line);
    EXPECT_TRUE(match.hasMatch()) << line.toStdString();
    out << match.captured(1) + ' ' + match.captured(2) + ' ' +
               match.captured(3);
  }
  return out;
}

std::vector<std::string> ToStd(const QStringList& list) {
  std::vector<std::string> out;
  for (const QString& s : list) {
    out.push_back(s.toStdString());
  }
  return out;
}

QString ReadFile(const QString& path) {
  QFile file(path);
  EXPECT_TRUE(file.open(QIODevice::ReadOnly)) << path.toStdString();
  return QString::fromUtf8(file.readAll());
}

// The same messages in the file and on the console.
TEST(CliLoggingTest, DebugLevelWritesSameMessagesToFileAndConsole) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString log = dir.filePath(QStringLiteral("cli.log"));
  const CliRun run =
      RunCli({QStringLiteral("--log-level"), QStringLiteral("debug"),
              QStringLiteral("--log-file"), log});
  ASSERT_EQ(run.exit_code, 0) << run.err.toStdString();
  const std::vector<std::string> console = ToStd(Messages(run.err));
  const std::vector<std::string> file = ToStd(Messages(ReadFile(log)));
  EXPECT_THAT(console, Not(IsEmpty()));
  EXPECT_THAT(file, UnorderedElementsAreArray(console));
  EXPECT_THAT(console, Contains(::testing::StartsWith("debug cli ")));
  EXPECT_THAT(console, Contains(::testing::StartsWith("info cli ")));
}

TEST(CliLoggingTest, InfoLevelLeavesOutDebugMessages) {
  const CliRun run = RunCli({});
  ASSERT_EQ(run.exit_code, 0);
  const std::vector<std::string> console = ToStd(Messages(run.err));
  EXPECT_THAT(console, Contains(::testing::StartsWith("info cli ")));
  EXPECT_THAT(console, Not(Contains(::testing::StartsWith("debug "))));
}

TEST(CliLoggingTest, NoConsoleLogLeavesOnlyTheFile) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const QString log = dir.filePath(QStringLiteral("cli.log"));
  const CliRun run = RunCli(
      {QStringLiteral("--log-level"), QStringLiteral("debug"),
       QStringLiteral("--log-file"), log, QStringLiteral("--no-console-log")});
  ASSERT_EQ(run.exit_code, 0);
  EXPECT_TRUE(run.err.isEmpty()) << run.err.toStdString();
  EXPECT_THAT(ToStd(Messages(ReadFile(log))), Not(IsEmpty()));
}

TEST(CliLoggingTest, NoConsoleLogWithoutFileIsRefused) {
  const CliRun run = RunCli({QStringLiteral("--no-console-log")});
  EXPECT_EQ(run.exit_code, 2);
  EXPECT_TRUE(run.err.contains(QStringLiteral("--log-file")));
}

TEST(CliLoggingTest, UnknownLevelIsRefused) {
  const CliRun run =
      RunCli({QStringLiteral("--log-level"), QStringLiteral("verbose")});
  EXPECT_EQ(run.exit_code, 2);
  EXPECT_TRUE(run.err.contains(QStringLiteral("verbose")));
}

}  // namespace
}  // namespace speak5220::cli
