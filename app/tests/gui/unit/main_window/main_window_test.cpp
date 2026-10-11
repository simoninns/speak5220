/*
 * File:        main_window_test.cpp
 * Module:      main_window tests
 * Purpose:     Unit tests for the application's top-level window, with
 *              mocked dialogs and no files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/main_window.h"

#include <QAction>
#include <QLineEdit>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <utility>
#include <vector>

#include "core/project/phrase.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/main_window/dialogs.h"
#include "gui/phrase_editor/phrase_details.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/support/gui_mocks.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using core::project::PhraseId;
using ::testing::_;
using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::FakeRecentFilesStore;
using testing::MockDialogs;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestId;

class MainWindowTest : public ::testing::Test {
 protected:
  // Adds phrases as one undoable step, as an import would.
  void AddPhrases(std::vector<core::project::Phrase> phrases) {
    std::vector<core::project::PlacedPhrase> placed;
    placed.reserve(phrases.size());
    for (core::project::Phrase& phrase : phrases) {
      placed.push_back({.index = static_cast<int>(placed.size()),
                        .phrase = std::move(phrase)});
    }
    ASSERT_TRUE(window_.Document().Edits().InsertPhrases(placed).IsOk());
  }

  StrictMock<MockDialogs> dialogs_;
  FakeRecentFilesStore recent_;
  core::InlineTaskRunner runner_;
  MainWindow window_{dialogs_, recent_, runner_};
};

TEST_F(MainWindowTest, NewProjectIsUntitledAndUnmodified) {
  EXPECT_EQ(window_.windowTitle(), QStringLiteral("Untitled[*] - Speak5220"));
  EXPECT_FALSE(window_.isWindowModified());
  EXPECT_EQ(window_.Model().PhraseCount(), 0);
  EXPECT_EQ(window_.StatusText(), QStringLiteral("No PHROMs in use"));
}

TEST_F(MainWindowTest, EditMarksTheProjectModifiedAndUndoClearsIt) {
  AddPhrases({EncodedPhrase(1, "HELLO", 1000)});
  EXPECT_TRUE(window_.isWindowModified());
  EXPECT_EQ(window_.Model().PhraseCount(), 1);
  EXPECT_EQ(window_.StatusText(),
            QStringLiteral("1.2 KB · 1 PHROM of 64 · 14.8 KB free "
                           "in PHROM 1"));
  window_.Document().UndoStack().undo();
  EXPECT_FALSE(window_.isWindowModified());
  EXPECT_EQ(window_.Model().PhraseCount(), 0);
}

TEST_F(MainWindowTest, SelectingOnePhraseShowsItsDetails) {
  AddPhrases({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(2)});
  EXPECT_EQ(window_.Details()->NameField()->text(), QStringLiteral("TWO"));
  window_.Panel()->Select({TestId(1), TestId(2)});
  EXPECT_FALSE(window_.Details()->NameField()->isEnabled());
}

TEST_F(MainWindowTest, SelectionFollowsAPhraseThatMoves) {
  AddPhrases({AudioPhrase(1, "CHARLIE"), AudioPhrase(2, "ALPHA"),
              AudioPhrase(3, "BRAVO")});
  window_.Panel()->Select({TestId(1)});
  ASSERT_TRUE(window_.Model().Sort());
  EXPECT_EQ(window_.Panel()->SelectedIds(), std::vector<PhraseId>{TestId(1)});
  EXPECT_EQ(window_.Panel()->SelectedRows(), QList<int>{2});
}

TEST_F(MainWindowTest, RemoveAsksFirst) {
  AddPhrases({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(2)});
  QAction* remove = nullptr;
  for (QAction* action : window_.findChildren<QAction*>()) {
    if (action->text() == QStringLiteral("&Remove")) {
      remove = action;
    }
  }
  ASSERT_NE(remove, nullptr);
  ASSERT_TRUE(remove->isEnabled());
  EXPECT_CALL(dialogs_, ConfirmRemove(QStringList{QStringLiteral("TWO")}))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  remove->trigger();
  EXPECT_EQ(window_.Model().PhraseCount(), 2);
  remove->trigger();
  EXPECT_EQ(window_.Model().PhraseCount(), 1);
}

TEST_F(MainWindowTest, ClosingWithUnsavedChangesCanBeCancelled) {
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, AskSaveChanges(QStringLiteral("Untitled")))
      .WillOnce(Return(SaveChoice::kCancel));
  EXPECT_FALSE(window_.close());
}

TEST_F(MainWindowTest, ClosingWithUnsavedChangesCanDiscardThem) {
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, AskSaveChanges(_))
      .WillOnce(Return(SaveChoice::kDiscard));
  EXPECT_TRUE(window_.close());
}

TEST_F(MainWindowTest, ClosingWhenSaveIsCancelledKeepsTheWindowOpen) {
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, AskSaveChanges(_)).WillOnce(Return(SaveChoice::kSave));
  EXPECT_CALL(dialogs_, SaveProjectPath(_)).WillOnce(Return(std::nullopt));
  EXPECT_FALSE(window_.close());
  EXPECT_TRUE(window_.isWindowModified());
}

TEST_F(MainWindowTest, ClosingAnUnmodifiedProjectAsksNothing) {
  EXPECT_TRUE(window_.close());
}

TEST_F(MainWindowTest, FailedListEditIsShown) {
  // A rename to a taken name, made through the details, is shown there; a
  // failed edit from the list goes to a dialog.
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Edit Phrases"), _));
  emit window_.Model().EditFailed(QStringLiteral("cannot move"));
}

}  // namespace
}  // namespace speak5220::app
