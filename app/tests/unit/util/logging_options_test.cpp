/*
 * File:        logging_options_test.cpp
 * Module:      util tests
 * Purpose:     Unit tests of the --log-level, --log-file and
 *              --no-console-log options and the defaults they override
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/logging_options.h"

#include <QCommandLineParser>
#include <QString>
#include <QStringList>

#include "core/util/logging.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

// The settings `arguments` give over `defaults`.
StatusOr<LogSettings> Parse(const QStringList& arguments,
                            const LogSettings& defaults = {}) {
  QCommandLineParser parser;
  AddLoggingOptions(parser);
  QStringList line = {QStringLiteral("speak5220")};
  line << arguments;
  if (!parser.parse(line)) {
    return InvalidArgumentError(parser.errorText().toStdString());
  }
  return LogSettingsFromParser(parser, defaults);
}

TEST(LoggingOptionsTest, NoOptionsGiveInfoOnTheConsole) {
  const StatusOr<LogSettings> settings = Parse({});
  ASSERT_TRUE(settings.IsOk());
  EXPECT_EQ(settings->level, LogLevel::kInfo);
  EXPECT_TRUE(settings->console);
  EXPECT_TRUE(settings->file_path.empty());
}

TEST(LoggingOptionsTest, NoOptionsKeepTheDefaults) {
  const LogSettings defaults{
      .level = LogLevel::kTrace, .console = true, .file_path = "saved.log"};
  const StatusOr<LogSettings> settings = Parse({}, defaults);
  ASSERT_TRUE(settings.IsOk());
  EXPECT_EQ(settings->level, LogLevel::kTrace);
  EXPECT_EQ(settings->file_path, "saved.log");
}

TEST(LoggingOptionsTest, OptionsOverrideTheDefaults) {
  const LogSettings defaults{
      .level = LogLevel::kTrace, .console = true, .file_path = "saved.log"};
  const StatusOr<LogSettings> settings =
      Parse({QStringLiteral("--log-level"), QStringLiteral("debug"),
             QStringLiteral("--log-file"), QStringLiteral("run.log")},
            defaults);
  ASSERT_TRUE(settings.IsOk());
  EXPECT_EQ(settings->level, LogLevel::kDebug);
  EXPECT_EQ(settings->file_path, "run.log");
}

TEST(LoggingOptionsTest, InfoOnTheCommandLineOverridesADefaultLevel) {
  const StatusOr<LogSettings> settings =
      Parse({QStringLiteral("--log-level"), QStringLiteral("info")},
            {.level = LogLevel::kTrace, .console = true, .file_path = ""});
  ASSERT_TRUE(settings.IsOk());
  EXPECT_EQ(settings->level, LogLevel::kInfo);
}

TEST(LoggingOptionsTest, NoConsoleLogNeedsAFile) {
  EXPECT_FALSE(Parse({QStringLiteral("--no-console-log")}).IsOk());
  const StatusOr<LogSettings> settings = Parse(
      {QStringLiteral("--no-console-log")},
      {.level = LogLevel::kInfo, .console = true, .file_path = "saved.log"});
  ASSERT_TRUE(settings.IsOk());
  EXPECT_FALSE(settings->console);
}

TEST(LoggingOptionsTest, UnknownLevelFails) {
  EXPECT_FALSE(
      Parse({QStringLiteral("--log-level"), QStringLiteral("loud")}).IsOk());
}

}  // namespace
}  // namespace speak5220::core
