/*
 * File:        grip_splitter_test.cpp
 * Module:      main_window tests
 * Purpose:     Paints the splitter's grip on the offscreen platform
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/grip_splitter.h"

#include <QApplication>
#include <QImage>
#include <QLabel>
#include <QPoint>
#include <QSplitterHandle>

#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

TEST(GripSplitterTest, HandleShowsAGripInItsMiddle) {
  GripSplitter splitter(Qt::Vertical);
  splitter.addWidget(new QLabel(QStringLiteral("above")));
  splitter.addWidget(new QLabel(QStringLiteral("below")));
  splitter.resize(300, 200);
  splitter.show();
  QApplication::processEvents();

  QSplitterHandle* handle = splitter.handle(1);
  ASSERT_NE(handle, nullptr);
  EXPECT_GE(handle->height(), 8);
  const QImage image = handle->grab().toImage();
  ASSERT_FALSE(image.isNull());
  // The grip is drawn at the centre; the handle's end is left plain.
  const QPoint centre(image.width() / 2, image.height() / 2);
  EXPECT_NE(image.pixelColor(centre), image.pixelColor(0, centre.y()));
}

}  // namespace
}  // namespace speak5220::app
