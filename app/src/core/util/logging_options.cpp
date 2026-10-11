/*
 * File:        logging_options.cpp
 * Module:      util
 * Purpose:     The --log-level, --log-file and --no-console-log options shared
 *              by both executables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/logging_options.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QString>

namespace speak5220::core {
namespace {

QString LevelOptionName() { return QStringLiteral("log-level"); }
QString FileOptionName() { return QStringLiteral("log-file"); }
QString NoConsoleOptionName() { return QStringLiteral("no-console-log"); }

}  // namespace

void AddLoggingOptions(QCommandLineParser& parser) {
  parser.addOption(QCommandLineOption(
      LevelOptionName(),
      QStringLiteral("Log level: info (default), debug or trace."),
      QStringLiteral("level"), QStringLiteral("info")));
  parser.addOption(QCommandLineOption(
      FileOptionName(),
      QStringLiteral("Also write the log to <path>, replacing the file."),
      QStringLiteral("path")));
  parser.addOption(QCommandLineOption(
      NoConsoleOptionName(),
      QStringLiteral("Turn console logging off (needs --log-file).")));
}

StatusOr<LogSettings> LogSettingsFromParser(const QCommandLineParser& parser) {
  LogSettings settings;
  StatusOr<LogLevel> level =
      ParseLogLevel(parser.value(LevelOptionName()).toStdString());
  if (!level.IsOk()) {
    return level.GetStatus();
  }
  settings.level = *level;
  settings.file_path = parser.value(FileOptionName()).toStdString();
  settings.console = !parser.isSet(NoConsoleOptionName());
  if (!settings.console && settings.file_path.empty()) {
    return InvalidArgumentError("--no-console-log needs --log-file");
  }
  return settings;
}

}  // namespace speak5220::core
