/*
 * File:        main_window.cpp
 * Module:      main_window
 * Purpose:     The application's top-level window
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/main_window.h"

#include <QMainWindow>
#include <QString>

#include "core/util/version.h"

namespace speak5220::app {

namespace {

// The window's initial size in device-independent pixels.
constexpr int kInitialWidth = 1024;
constexpr int kInitialHeight = 640;

}  // namespace

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
  const std::string_view name = core::kApplicationName;
  setWindowTitle(
      QString::fromUtf8(name.data(), static_cast<qsizetype>(name.size())));
  resize(kInitialWidth, kInitialHeight);
}

}  // namespace speak5220::app
