/*
 * File:        gui_test_main.cc
 * Module:      gui tests
 * Purpose:     Runs the GUI tests inside a QApplication on the offscreen
 * platform
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>
#include <QByteArray>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

int main(int argc, char* argv[]) {
  // TESTING.md: GUI tests run on the offscreen platform, so they need no
  // display. An explicit QT_QPA_PLATFORM from the caller still wins.
  if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM")) {
    qputenv("QT_QPA_PLATFORM", QByteArray("offscreen"));
  }
  testing::InitGoogleMock(&argc, argv);
  const QApplication application(argc, argv);
  return RUN_ALL_TESTS();
}
