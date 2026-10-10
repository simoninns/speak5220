/*
 * File:        logging.cc
 * Module:      util
 * Purpose:     Sets up the spdlog sinks and per-module loggers shared by both
 *              executables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/logging.h"

#include <spdlog/async.h>
#include <spdlog/async_logger.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace speak5220::core {
namespace {

// Console lines: level and module, no timestamp.
constexpr std::string_view kConsolePattern = "[%^%l%$] [%n] %v";
// File lines: timestamp, level, thread and module (plan §2.6).
constexpr std::string_view kFilePattern =
    "%Y-%m-%d %H:%M:%S.%e [%l] [thread %t] [%n] %v";
// Messages queued for the asynchronous file logger before a caller waits.
constexpr std::size_t kAsyncQueueSize = 8192;
// How often the log file is flushed, so a process that is killed or crashes
// loses at most this much of its log.
constexpr std::chrono::seconds kFlushInterval{1};

// The current set-up. Guarded by the mutex; the sinks themselves are the
// thread-safe (_mt) variants, so loggers may be used from any thread.
struct LoggingState {
  std::mutex mutex;
  LogLevel level = LogLevel::kInfo;
  std::vector<spdlog::sink_ptr> sinks;
  bool asynchronous = false;
  bool configured = false;
};

LoggingState& State() {
  static LoggingState state;
  return state;
}

spdlog::level::level_enum ToSpdlogLevel(LogLevel level) {
  switch (level) {
    case LogLevel::kInfo:
      return spdlog::level::info;
    case LogLevel::kDebug:
      return spdlog::level::debug;
    case LogLevel::kTrace:
      return spdlog::level::trace;
  }
  return spdlog::level::info;
}

spdlog::sink_ptr ConsoleSink() {
  auto sink = std::make_shared<spdlog::sinks::stderr_color_sink_mt>();
  sink->set_pattern(std::string(kConsolePattern));
  return sink;
}

// Installs a new set-up. The caller holds the state's mutex.
void InstallLocked(LoggingState& state, LogLevel level,
                   std::vector<spdlog::sink_ptr> sinks, bool asynchronous) {
  spdlog::drop_all();
  state.level = level;
  state.sinks = std::move(sinks);
  state.asynchronous = asynchronous;
  state.configured = true;
}

}  // namespace

std::string_view LogLevelName(LogLevel level) {
  switch (level) {
    case LogLevel::kInfo:
      return "info";
    case LogLevel::kDebug:
      return "debug";
    case LogLevel::kTrace:
      return "trace";
  }
  return "info";
}

StatusOr<LogLevel> ParseLogLevel(std::string_view name) {
  for (const LogLevel level :
       {LogLevel::kInfo, LogLevel::kDebug, LogLevel::kTrace}) {
    if (name == LogLevelName(level)) {
      return level;
    }
  }
  return InvalidArgumentError("unknown log level \"" + std::string(name) +
                              "\" (expected info, debug or trace)");
}

Status ConfigureLogging(const LogSettings& settings) {
  if (!settings.console && settings.file_path.empty()) {
    return InvalidArgumentError(
        "console logging can be turned off only when a log file is given");
  }
  std::vector<spdlog::sink_ptr> sinks;
  if (settings.console) {
    sinks.push_back(ConsoleSink());
  }
  if (!settings.file_path.empty()) {
    // spdlog reports a file it cannot open by throwing; exceptions from
    // third-party code are caught here, at the boundary (plan §1.3).
    try {
      auto file = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
          settings.file_path, /*truncate=*/true);
      file->set_pattern(std::string(kFilePattern));
      sinks.push_back(std::move(file));
    } catch (const spdlog::spdlog_ex& error) {
      return UnavailableError("cannot open log file " + settings.file_path +
                              ": " + error.what());
    }
  }
  const bool asynchronous = !settings.file_path.empty();
  if (asynchronous && spdlog::thread_pool() == nullptr) {
    spdlog::init_thread_pool(kAsyncQueueSize, 1);
  }
  if (asynchronous) {
    spdlog::flush_every(kFlushInterval);
  }
  LoggingState& state = State();
  const std::scoped_lock lock(state.mutex);
  InstallLocked(state, settings.level, std::move(sinks), asynchronous);
  return OkStatus();
}

void ConfigureLoggingWithSinks(LogLevel level,
                               std::vector<spdlog::sink_ptr> sinks) {
  LoggingState& state = State();
  const std::scoped_lock lock(state.mutex);
  InstallLocked(state, level, std::move(sinks), /*asynchronous=*/false);
}

std::shared_ptr<spdlog::logger> Logger(std::string_view module) {
  LoggingState& state = State();
  const std::scoped_lock lock(state.mutex);
  if (!state.configured) {
    // Before any set-up: the default of plan §2.6, info to the console.
    InstallLocked(state, LogLevel::kInfo, {ConsoleSink()}, false);
  }
  const std::string name(module);
  if (auto existing = spdlog::get(name)) {
    return existing;
  }
  std::shared_ptr<spdlog::logger> logger;
  if (state.asynchronous) {
    // Blocks rather than drops when the queue is full: logging never loses
    // a message, whatever the level.
    logger = std::make_shared<spdlog::async_logger>(
        name, state.sinks.begin(), state.sinks.end(), spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);
  } else {
    logger = std::make_shared<spdlog::logger>(name, state.sinks.begin(),
                                              state.sinks.end());
  }
  logger->set_level(ToSpdlogLevel(state.level));
  logger->flush_on(spdlog::level::warn);
  spdlog::register_logger(logger);
  return logger;
}

void ShutdownLogging() {
  LoggingState& state = State();
  const std::scoped_lock lock(state.mutex);
  spdlog::apply_all(
      [](const std::shared_ptr<spdlog::logger>& logger) { logger->flush(); });
  spdlog::shutdown();
  state.sinks.clear();
  state.configured = false;
}

}  // namespace speak5220::core
