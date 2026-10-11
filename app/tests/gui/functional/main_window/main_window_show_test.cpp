/*
 * File:        main_window_show_test.cpp
 * Module:      main_window tests
 * Purpose:     Shows the top-level window on the offscreen platform
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>
#include <QListView>
#include <QPixmap>

#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/main_window/main_window.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/support/gui_mocks.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using testing::EncodedPhrase;
using testing::FakeRecentFilesStore;
using testing::MockDialogs;
using ::testing::NiceMock;
using testing::TestPlayback;

TEST(MainWindowShowTest, BecomesVisibleWhenShown) {
  NiceMock<MockDialogs> dialogs;
  FakeRecentFilesStore recent;
  core::InlineTaskRunner runner;
  TestPlayback playback;
  testing::TestEncoding encoding;
  MainWindow window(dialogs, recent, runner, playback.synth, playback.player,
                    encoding.Tools());
  window.show();
  QApplication::processEvents();
  EXPECT_TRUE(window.isVisible());
}

TEST(MainWindowShowTest, PaintsThePhraseListWithDividers) {
  NiceMock<MockDialogs> dialogs;
  FakeRecentFilesStore recent;
  core::InlineTaskRunner runner;
  TestPlayback playback;
  testing::TestEncoding encoding;
  MainWindow window(dialogs, recent, runner, playback.synth, playback.player,
                    encoding.Tools());
  ASSERT_TRUE(window.Document()
                  .Edits()
                  .InsertPhrases(
                      {{.index = 0, .phrase = EncodedPhrase(1, "ONE", 9000)},
                       {.index = 1, .phrase = EncodedPhrase(2, "TWO", 9000)},
                       {.index = 2, .phrase = EncodedPhrase(3, "BIG", 17000)}})
                  .IsOk());
  window.show();
  QApplication::processEvents();
  QListView* view = window.Panel()->View();
  // The row that starts PHROM 2 is taller: it carries the divider.
  const int plain = view->visualRect(window.Model().index(0)).height();
  const int divided = view->visualRect(window.Model().index(1)).height();
  EXPECT_GT(divided, plain);
  const QPixmap image = window.grab();
  EXPECT_FALSE(image.isNull());
}

}  // namespace
}  // namespace speak5220::app
