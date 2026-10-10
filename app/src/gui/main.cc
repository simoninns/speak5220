/*
 * File:        main.cc
 * Module:      gui
 * Purpose:     Entry point of the speak5220 GUI executable
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>
#include <QString>

#include "core/util/version.h"
#include "gui/main_window/main_window.h"

namespace {

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

int main(int argc, char* argv[]) {
  QApplication application(argc, argv);
  QApplication::setApplicationName(
      ToQString(speak5220::core::kApplicationName));
  QApplication::setApplicationVersion(ToQString(speak5220::core::Version()));

  speak5220::app::MainWindow window;
  window.show();
  return QApplication::exec();
}
