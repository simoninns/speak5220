/*
 * File:        logging.h
 * Module:      util
 * Purpose:     Sets up the spdlog sinks and per-module loggers shared by both
 *              executables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_LOGGING_H_
#define SPEAK5220_CORE_UTIL_LOGGING_H_

#include <spdlog/common.h>
#include <spdlog/logger.h>

#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core {

// The selectable levels of plan §2.6. Warnings and errors are logged at
// every level.
enum class LogLevel {
  kInfo,   // lifecycle messages, stage transitions, validation summaries
  kDebug,  // plus configuration, branch decisions, paths, encoder options
  kTrace,  // plus per-frame, per-sample and per-slot state
};

// "info", "debug" or "trace".
std::string_view LogLevelName(LogLevel level);

// Parses a level name as written by LogLevelName().
StatusOr<LogLevel> ParseLogLevel(std::string_view name);

// The destinations and level chosen on the command line or in preferences.
struct LogSettings {
  LogLevel level = LogLevel::kInfo;
  // Console output to stderr, coloured when it is a terminal.
  bool console = true;
  // Also write every message to this file, replacing it. Empty: no file.
  std::string file_path;
};

// Module names used by the core and the front ends. Any name may be used;
// these are the ones the plan lists.
inline constexpr std::string_view kLogModuleCli = "cli";
inline constexpr std::string_view kLogModuleGui = "gui";
inline constexpr std::string_view kLogModuleEmulator = "emulator";
inline constexpr std::string_view kLogModuleSynth = "synth";
inline constexpr std::string_view kLogModuleAudio = "audio";
inline constexpr std::string_view kLogModuleEncoder = "encoder";
inline constexpr std::string_view kLogModuleQuality = "quality";

// Replaces the logging set-up with a console sink, a file sink, or both.
// File output goes through spdlog's asynchronous logger, so trace output
// from hot loops does not wait for the disk. Fails, leaving logging
// unchanged, when the file cannot be opened or no destination is left.
// Call before starting worker threads; loggers obtained earlier keep the
// old sinks.
Status ConfigureLogging(const LogSettings& settings);

// Replaces the logging set-up with the given sinks, synchronously. Used by
// tests (an in-memory ringbuffer sink) and by front ends that add their own
// sink. Same threading rule as ConfigureLogging().
void ConfigureLoggingWithSinks(LogLevel level,
                               std::vector<spdlog::sink_ptr> sinks);

// The logger for `module`, created on first use with the current sinks.
// Thread-safe. Each line names the module. Hot loops should fetch the
// logger once and guard trace calls with should_log(), so a lower level
// costs almost nothing.
std::shared_ptr<spdlog::logger> Logger(std::string_view module);

// Flushes every logger and stops the asynchronous worker. Call once before
// the process exits.
void ShutdownLogging();

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_LOGGING_H_
