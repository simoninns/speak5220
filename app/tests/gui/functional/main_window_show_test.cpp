/*
 * File:        main_window_show_test.cpp
 * Module:      main_window tests
 * Purpose:     Shows the top-level window on the offscreen platform
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>

#include "gtest/gtest.h"
#include "gui/main_window/main_window.h"

namespace speak5220::app {
namespace {

TEST(MainWindowShowTest, BecomesVisibleWhenShown) {
  MainWindow window;
  window.show();
  QApplication::processEvents();
  EXPECT_TRUE(window.isVisible());
}

}  // namespace
}  // namespace speak5220::app
