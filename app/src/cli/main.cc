/*
 * File:        main.cc
 * Module:      cli
 * Purpose:     Entry point of the headless speak5220-cli executable
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QString>
#include <QTextStream>
#include <string_view>

#include "core/util/logging.h"
#include "core/util/logging_options.h"
#include "core/util/status.h"
#include "core/util/version.h"

namespace {

// Exit code for a command line that cannot be used.
constexpr int kUsageExitCode = 2;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace core = speak5220::core;
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("speak5220-cli"));
  QCoreApplication::setApplicationVersion(ToQString(core::Version()));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Creates TMS5220 speech data and builds PHROMs."));
  parser.addHelpOption();
  parser.addVersionOption();
  core::AddLoggingOptions(parser);
  parser.process(application);

  const core::StatusOr<core::LogSettings> settings =
      core::LogSettingsFromParser(parser);
  if (!settings.IsOk()) {
    QTextStream(stderr) << "speak5220-cli: "
                        << ToQString(settings.GetStatus().Message()) << '\n';
    return kUsageExitCode;
  }
  if (const core::Status status = core::ConfigureLogging(*settings);
      !status.IsOk()) {
    QTextStream(stderr) << "speak5220-cli: " << ToQString(status.Message())
                        << '\n';
    return kUsageExitCode;
  }
  const auto log = core::Logger(core::kLogModuleCli);
  log->info("speak5220-cli {} started", core::Version());
  log->debug("log level {}, console {}, log file \"{}\"",
             core::LogLevelName(settings->level),
             settings->console ? "on" : "off", settings->file_path);

  // There are no commands yet, so a plain run reports the version.
  QTextStream(stdout) << QCoreApplication::applicationName() << ' '
                      << QCoreApplication::applicationVersion() << '\n';

  log->info("speak5220-cli finished");
  core::ShutdownLogging();
  return 0;
}
