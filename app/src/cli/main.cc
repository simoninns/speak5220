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

#include "core/util/version.h"

namespace {

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

int main(int argc, char* argv[]) {
  QCoreApplication application(argc, argv);
  QCoreApplication::setApplicationName(QStringLiteral("speak5220-cli"));
  QCoreApplication::setApplicationVersion(
      ToQString(speak5220::core::Version()));

  QCommandLineParser parser;
  parser.setApplicationDescription(
      QStringLiteral("Creates TMS5220 speech data and builds PHROMs."));
  parser.addHelpOption();
  parser.addVersionOption();
  parser.process(application);

  // There are no commands yet, so a plain run reports the version.
  QTextStream(stdout) << QCoreApplication::applicationName() << ' '
                      << QCoreApplication::applicationVersion() << '\n';
  return 0;
}
