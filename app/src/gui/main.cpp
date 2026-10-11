/*
 * File:        main.cpp
 * Module:      gui
 * Purpose:     Entry point of the speak5220 GUI executable
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QString>
#include <QTextStream>
#include <string_view>

#include "core/util/logging.h"
#include "core/util/logging_options.h"
#include "core/util/status.h"
#include "core/util/version.h"
#include "gui/main_window/main_window.h"

namespace {

// Exit code for a command line that cannot be used.
constexpr int kUsageExitCode = 2;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace core = speak5220::core;
  QApplication application(argc, argv);
  QApplication::setApplicationName(ToQString(core::kApplicationName));
  QApplication::setApplicationVersion(ToQString(core::Version()));

  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();
  core::AddLoggingOptions(parser);
  parser.process(application);

  const core::StatusOr<core::LogSettings> settings =
      core::LogSettingsFromParser(parser);
  if (!settings.IsOk()) {
    QTextStream(stderr) << "speak5220: "
                        << ToQString(settings.GetStatus().Message()) << '\n';
    return kUsageExitCode;
  }
  if (const core::Status status = core::ConfigureLogging(*settings);
      !status.IsOk()) {
    QTextStream(stderr) << "speak5220: " << ToQString(status.Message()) << '\n';
    return kUsageExitCode;
  }
  core::Logger(core::kLogModuleGui)
      ->info("speak5220 {} started", core::Version());

  speak5220::app::MainWindow window;
  window.show();
  const int result = QApplication::exec();
  core::Logger(core::kLogModuleGui)->info("speak5220 finished");
  core::ShutdownLogging();
  return result;
}
