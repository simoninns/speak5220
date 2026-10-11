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
#include <QCheckBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QStringList>
#include <QToolButton>
#include <QUndoStack>
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "core/project/phrase.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/encoder_panel.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/main_window/dialogs.h"
#include "gui/optimise/optimise_dialog.h"
#include "gui/phrase_editor/phrase_details.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/support/gui_mocks.h"
#include "gui/waveform/phrase_waveforms.h"
#include "gui/waveform/waveform_pane.h"
#include "gui/waveform/waveform_view.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using core::project::PhraseId;
using ::testing::_;
using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::FakeRecentFilesStore;
using testing::FakeViewPreferences;
using testing::MockDialogs;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestId;
using testing::TestPlayback;

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
  FakeViewPreferences preferences_;
  core::InlineTaskRunner runner_;
  TestPlayback playback_;
  testing::TestEncoding encoding_;
  MainWindow window_{dialogs_,         recent_,         preferences_,
                     runner_,          playback_.synth, playback_.player,
                     encoding_.Tools()};
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

TEST_F(MainWindowTest, SelectingAPhraseShowsItsEncoding) {
  AddPhrases({EncodedPhrase(1, "ONE", 100), AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(1)});
  QCoreApplication::processEvents();
  EXPECT_TRUE(window_.Encoder()->FramesLabel()->text().contains(
      QStringLiteral("frames")));
  EXPECT_TRUE(window_.Encoder()->ReEncodeButton()->isEnabled());
  window_.Panel()->Select({TestId(1), TestId(2)});
  EXPECT_FALSE(window_.Encoder()->ReEncodeButton()->isEnabled());
}

TEST_F(MainWindowTest, RemembersWhetherTheAdvancedSettingsAreShown) {
  EXPECT_FALSE(window_.Encoder()->Form()->AdvancedShown());
  window_.Encoder()->Form()->AdvancedButton()->click();
  EXPECT_TRUE(preferences_.advanced_shown);

  FakeViewPreferences shown;
  shown.advanced_shown = true;
  MainWindow window(dialogs_, recent_, shown, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_TRUE(window.Encoder()->Form()->AdvancedShown());
}

TEST_F(MainWindowTest, TurningTheSquelchOnShadesTheOriginalAudio) {
  // Loud, then 60 dB quieter, a tenth of a second each.
  core::project::Phrase breath = AudioPhrase(1, "BREATH");
  breath.audio.assign(1600, 0.5F);
  std::fill(breath.audio.begin() + 800, breath.audio.end(), 0.0005F);
  AddPhrases({breath, AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(1)});
  WaveformView* original = window_.Waveforms()->OriginalPane()->View();
  EXPECT_TRUE(original->Shading().empty());
  window_.Encoder()->Form()->OnBox("squelch_db")->setChecked(true);
  EXPECT_EQ(window_.Waveforms()->Squelch(), 30.0);
  EXPECT_FALSE(original->Shading().empty());
  // Selecting several phrases shows none, and so no squelch.
  window_.Panel()->Select({TestId(1), TestId(2)});
  EXPECT_EQ(window_.Waveforms()->Squelch(), std::nullopt);
  EXPECT_TRUE(original->Shading().empty());
}

TEST_F(MainWindowTest, ReEncodeEncodesTheSelectionAsOneStep) {
  AddPhrases({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO"),
              testing::LockedPhrase(3, "THREE", 50)});
  window_.Panel()->Select({TestId(1), TestId(2), TestId(3)});
  const int steps = window_.Document().UndoStack().count();
  window_.ReEncodeSelected();
  QCoreApplication::processEvents();
  EXPECT_EQ(window_.EncodesRunning(), 0);
  const core::project::Project& project = window_.Document().Project();
  ASSERT_TRUE(project.FindPhrase(TestId(1))->encoding.has_value());
  ASSERT_TRUE(project.FindPhrase(TestId(2))->encoding.has_value());
  EXPECT_EQ(project.FindPhrase(TestId(2))->encoding->pesq,
            testing::TestEncoding::kScore);
  EXPECT_EQ(window_.Document().UndoStack().count(), steps + 1);
  EXPECT_EQ(window_.Document().UndoStack().undoText(),
            QStringLiteral("Re-encode 2 Phrases"));
}

TEST_F(MainWindowTest, ReEncodeIsOfferedOnlyForStaleOrUnencodedPhrases) {
  QAction* re_encode = nullptr;
  for (QAction* action : window_.findChildren<QAction*>()) {
    if (action->text() == QStringLiteral("Re-&encode")) {
      re_encode = action;
    }
  }
  ASSERT_NE(re_encode, nullptr);
  AddPhrases({AudioPhrase(1, "ONE"), testing::LockedPhrase(2, "TWO", 50)});
  window_.Panel()->Select({TestId(2)});
  EXPECT_FALSE(re_encode->isEnabled());
  window_.Panel()->Select({TestId(1)});
  EXPECT_TRUE(re_encode->isEnabled());
  window_.ReEncodeSelected();
  QCoreApplication::processEvents();
  EXPECT_FALSE(re_encode->isEnabled());
  const int steps = window_.Document().UndoStack().count();
  window_.ReEncodeSelected();
  QCoreApplication::processEvents();
  EXPECT_EQ(window_.Document().UndoStack().count(), steps);
}

TEST_F(MainWindowTest, OptimiseIsOfferedForPhrasesThatCanBeEncoded) {
  AddPhrases({AudioPhrase(1, "ONE"), testing::LockedPhrase(2, "TWO", 50)});
  window_.Panel()->Select({TestId(2)});
  EXPECT_FALSE(window_.Encoder()->OptimiseButton()->isEnabled());
  window_.Panel()->Select({TestId(1)});
  EXPECT_TRUE(window_.Encoder()->OptimiseButton()->isEnabled());
  window_.Encoder()->OptimiseButton()->click();
  EXPECT_TRUE(window_.OptimiseWindow()->isVisible());
  EXPECT_EQ(window_.OptimiseWindow()->windowTitle(),
            QStringLiteral("Optimise ONE"));
  window_.OptimiseWindow()->reject();
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
