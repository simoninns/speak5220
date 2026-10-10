/*
 * File:        main_window_test.cc
 * Module:      main_window tests
 * Purpose:     Unit tests for the application's top-level window
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/main_window.h"

#include <QString>

#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

TEST(MainWindowTest, TitleIsApplicationName) {
  const MainWindow window;
  EXPECT_EQ(window.windowTitle(), QStringLiteral("Speak5220"));
}

}  // namespace
}  // namespace speak5220::app
