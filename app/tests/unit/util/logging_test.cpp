/*
 * File:        logging_test.cpp
 * Module:      util tests
 * Purpose:     Unit tests of the logging levels and per-module loggers,
 *              with an in-memory sink
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/logging.h"

#include <spdlog/pattern_formatter.h>
#include <spdlog/sinks/ringbuffer_sink.h>

#include <memory>
#include <string>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

using ::testing::ElementsAre;
using ::testing::ElementsAreArray;
using ::testing::IsEmpty;

constexpr std::size_t kRingSize = 64;

// Formats the sink's lines with `pattern` and no line end.
void SetPattern(spdlog::sinks::sink& sink, const std::string& pattern) {
  sink.set_formatter(std::make_unique<spdlog::pattern_formatter>(
      pattern, spdlog::pattern_time_type::local, ""));
}

// Logs one message at every spdlog level through `module` and returns the
// formatted lines that reached the sink.
std::vector<std::string> LogEveryLevel(LogLevel level,
                                       const std::string& module) {
  auto ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(kRingSize);
  SetPattern(*ring, "%l %n %v");
  ConfigureLoggingWithSinks(level, {ring});
  const auto log = Logger(module);
  log->trace("t");
  log->debug("d");
  log->info("i");
  log->warn("w");
  log->error("e");
  return ring->last_formatted();
}

class LoggingTest : public ::testing::Test {
 protected:
  void TearDown() override { ConfigureLoggingWithSinks(LogLevel::kInfo, {}); }
};

TEST_F(LoggingTest, InfoPassesInfoWarningsAndErrors) {
  EXPECT_THAT(LogEveryLevel(LogLevel::kInfo, "test"),
              ElementsAre("info test i", "warning test w", "error test e"));
}

TEST_F(LoggingTest, DebugAddsDebugMessages) {
  EXPECT_THAT(LogEveryLevel(LogLevel::kDebug, "test"),
              ElementsAre("debug test d", "info test i", "warning test w",
                          "error test e"));
}

TEST_F(LoggingTest, TraceAddsTraceMessages) {
  EXPECT_THAT(LogEveryLevel(LogLevel::kTrace, "test"),
              ElementsAre("trace test t", "debug test d", "info test i",
                          "warning test w", "error test e"));
}

TEST_F(LoggingTest, WarningsAndErrorsPassAtEveryLevel) {
  for (const LogLevel level :
       {LogLevel::kInfo, LogLevel::kDebug, LogLevel::kTrace}) {
    const std::vector<std::string> lines = LogEveryLevel(level, "test");
    ASSERT_GE(lines.size(), 2U);
    EXPECT_THAT(std::vector<std::string>(lines.end() - 2, lines.end()),
                ElementsAreArray({"warning test w", "error test e"}));
  }
}

TEST_F(LoggingTest, EachLineNamesItsModule) {
  auto ring = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(kRingSize);
  SetPattern(*ring, "[%n] %v");
  ConfigureLoggingWithSinks(LogLevel::kInfo, {ring});
  Logger(kLogModuleEmulator)->info("a");
  Logger(kLogModuleSynth)->info("b");
  EXPECT_THAT(ring->last_formatted(), ElementsAre("[emulator] a", "[synth] b"));
}

TEST_F(LoggingTest, SameModuleGivesSameLogger) {
  ConfigureLoggingWithSinks(LogLevel::kInfo, {});
  EXPECT_EQ(Logger("cli"), Logger("cli"));
}

TEST_F(LoggingTest, ReconfiguringReplacesSinks) {
  auto first = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(kRingSize);
  auto second = std::make_shared<spdlog::sinks::ringbuffer_sink_mt>(kRingSize);
  ConfigureLoggingWithSinks(LogLevel::kInfo, {first});
  ConfigureLoggingWithSinks(LogLevel::kInfo, {second});
  Logger("test")->info("x");
  EXPECT_THAT(first->last_formatted(), IsEmpty());
  EXPECT_EQ(second->last_formatted().size(), 1U);
}

TEST(LogLevelTest, ParsesEveryName) {
  for (const LogLevel level :
       {LogLevel::kInfo, LogLevel::kDebug, LogLevel::kTrace}) {
    const StatusOr<LogLevel> parsed = ParseLogLevel(LogLevelName(level));
    ASSERT_TRUE(parsed.IsOk());
    EXPECT_EQ(*parsed, level);
  }
}

TEST(LogLevelTest, RejectsUnknownName) {
  const StatusOr<LogLevel> parsed = ParseLogLevel("verbose");
  ASSERT_FALSE(parsed.IsOk());
  EXPECT_EQ(parsed.GetStatus().Code(), StatusCode::kInvalidArgument);
}

TEST(ConfigureLoggingTest, RejectsNoDestination) {
  const Status status = ConfigureLogging(
      {.level = LogLevel::kInfo, .console = false, .file_path = ""});
  EXPECT_EQ(status.Code(), StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace speak5220::core
