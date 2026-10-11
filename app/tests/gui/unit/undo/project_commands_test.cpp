/*
 * File:        project_commands_test.cpp
 * Module:      undo tests
 * Purpose:     Unit tests for the undo commands and ProjectEdits over a
 *              mocked IProjectEditor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/undo/project_commands.h"

#include <QUndoStack>
#include <string>
#include <vector>

#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_editor.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/support/gui_mocks.h"
#include "gui/undo/project_edits.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"

namespace speak5220::app {
namespace {

using core::project::EncodingChange;
using core::project::PhraseId;
using core::project::PlacedPhrase;
using ::testing::_;
using testing::AudioPhrase;
using ::testing::Eq;
using testing::IdsAre;
using ::testing::InSequence;
using testing::MockProjectEditor;
using testing::ProjectWith;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestId;

class ProjectCommandsTest : public ::testing::Test {
 protected:
  StrictMock<MockProjectEditor> editor_;
  QUndoStack stack_;
  ProjectEdits edits_{editor_, stack_};
};

TEST_F(ProjectCommandsTest, InsertRunsOnceWhenPushedAndUndoRemovesThem) {
  const std::vector<PlacedPhrase> placed = {
      {.index = 0, .phrase = AudioPhrase(1, "ONE")},
      {.index = 1, .phrase = AudioPhrase(2, "TWO")}};
  {
    InSequence sequence;
    EXPECT_CALL(editor_, InsertPhrases(Eq(placed)))
        .WillOnce(Return(core::OkStatus()));
    EXPECT_CALL(
        editor_,
        RemovePhrases(IdsAre(std::vector<PhraseId>{TestId(1), TestId(2)})))
        .WillOnce(Return(placed));
    EXPECT_CALL(editor_, InsertPhrases(Eq(placed)))
        .WillOnce(Return(core::OkStatus()));
  }
  ASSERT_TRUE(edits_.InsertPhrases(placed).IsOk());
  EXPECT_EQ(stack_.count(), 1);
  EXPECT_EQ(stack_.undoText(), QStringLiteral("Add 2 Phrases"));
  stack_.undo();
  stack_.redo();
  EXPECT_EQ(stack_.index(), 1);
}

TEST_F(ProjectCommandsTest, RemoveIsUndoneByInsertingWhatWasRemoved) {
  const std::vector<PhraseId> ids = {TestId(3)};
  const std::vector<PlacedPhrase> removed = {
      {.index = 2, .phrase = AudioPhrase(3, "THREE")}};
  {
    InSequence sequence;
    EXPECT_CALL(editor_, RemovePhrases(IdsAre(ids))).WillOnce(Return(removed));
    EXPECT_CALL(editor_, InsertPhrases(Eq(removed)))
        .WillOnce(Return(core::OkStatus()));
    EXPECT_CALL(editor_, RemovePhrases(IdsAre(ids))).WillOnce(Return(removed));
  }
  ASSERT_TRUE(edits_.RemovePhrases(ids).IsOk());
  EXPECT_EQ(stack_.undoText(), QStringLiteral("Remove Phrase"));
  stack_.undo();
  stack_.redo();
}

TEST_F(ProjectCommandsTest, MoveIsUndoneBySettingThePreviousOrder) {
  const std::vector<PhraseId> moved = {TestId(3)};
  const std::vector<PhraseId> previous = {TestId(1), TestId(2), TestId(3)};
  {
    InSequence sequence;
    EXPECT_CALL(editor_, MovePhrases(IdsAre(moved), 0))
        .WillOnce(Return(previous));
    EXPECT_CALL(editor_, SetOrder(IdsAre(previous)))
        .WillOnce(
            Return(std::vector<PhraseId>{TestId(3), TestId(1), TestId(2)}));
    EXPECT_CALL(editor_, MovePhrases(IdsAre(moved), 0))
        .WillOnce(Return(previous));
  }
  ASSERT_TRUE(edits_.MovePhrases(moved, 0).IsOk());
  stack_.undo();
  stack_.redo();
}

TEST_F(ProjectCommandsTest, SortIsUndoneBySettingThePreviousOrder) {
  const std::vector<PhraseId> previous = {TestId(2), TestId(1)};
  {
    InSequence sequence;
    EXPECT_CALL(editor_, SortPhrases()).WillOnce(Return(previous));
    EXPECT_CALL(editor_, SetOrder(IdsAre(previous)))
        .WillOnce(Return(std::vector<PhraseId>{TestId(1), TestId(2)}));
  }
  ASSERT_TRUE(edits_.SortPhrases().IsOk());
  EXPECT_EQ(stack_.undoText(), QStringLiteral("Sort Phrases A-Z"));
  stack_.undo();
}

TEST_F(ProjectCommandsTest, RenameIsUndoneByRenamingBack) {
  {
    InSequence sequence;
    EXPECT_CALL(editor_, RenamePhrase(TestId(1), Eq("NEW")))
        .WillOnce(Return(std::string("OLD")));
    EXPECT_CALL(editor_, RenamePhrase(TestId(1), Eq("OLD")))
        .WillOnce(Return(std::string("NEW")));
  }
  ASSERT_TRUE(edits_.RenamePhrase(TestId(1), "NEW").IsOk());
  EXPECT_EQ(stack_.undoText(), QStringLiteral("Rename to NEW"));
  stack_.undo();
}

TEST_F(ProjectCommandsTest, DescriptionIsUndoneBySettingThePreviousText) {
  {
    InSequence sequence;
    EXPECT_CALL(editor_, SetDescription(TestId(1), "new text"))
        .WillOnce(Return(std::string("old text")));
    EXPECT_CALL(editor_, SetDescription(TestId(1), "old text"))
        .WillOnce(Return(std::string("new text")));
  }
  ASSERT_TRUE(edits_.SetDescription(TestId(1), "new text").IsOk());
  stack_.undo();
}

TEST_F(ProjectCommandsTest, EncodingIsUndoneBySettingThePreviousOne) {
  const EncodingChange change{.overrides = {{"gate_db", "40"}},
                              .encoding = std::nullopt};
  const EncodingChange previous{.overrides = {}, .encoding = std::nullopt};
  {
    InSequence sequence;
    EXPECT_CALL(editor_, SetEncoding(TestId(1), Eq(change)))
        .WillOnce(Return(previous));
    EXPECT_CALL(editor_, SetEncoding(TestId(1), Eq(previous)))
        .WillOnce(Return(change));
  }
  ASSERT_TRUE(edits_.SetEncoding(TestId(1), change).IsOk());
  stack_.undo();
}

TEST_F(ProjectCommandsTest, FailedOperationIsNotPushed) {
  EXPECT_CALL(editor_, RenamePhrase(TestId(1), Eq("TAKEN")))
      .WillOnce(Return(core::InvalidArgumentError(
          "there is already a phrase called TAKEN")));
  const core::Status status = edits_.RenamePhrase(TestId(1), "TAKEN");
  EXPECT_EQ(status.Message(), "there is already a phrase called TAKEN");
  EXPECT_EQ(stack_.count(), 0);
}

TEST_F(ProjectCommandsTest, FailedOperationKeepsTheRedoHistory) {
  EXPECT_CALL(editor_, SortPhrases())
      .WillOnce(Return(std::vector<PhraseId>{TestId(2), TestId(1)}))
      .WillOnce(Return(core::FailedPreconditionError("budget")));
  EXPECT_CALL(editor_, SetOrder(_))
      .WillOnce(Return(std::vector<PhraseId>{TestId(1), TestId(2)}));
  ASSERT_TRUE(edits_.SortPhrases().IsOk());
  stack_.undo();
  ASSERT_TRUE(stack_.canRedo());
  EXPECT_FALSE(edits_.SortPhrases().IsOk());
  EXPECT_TRUE(stack_.canRedo());
  EXPECT_EQ(stack_.count(), 1);
}

TEST_F(ProjectCommandsTest, CommandThatCannotBeUndoneIsDropped) {
  EXPECT_CALL(editor_, RenamePhrase(TestId(1), Eq("NEW")))
      .WillOnce(Return(std::string("OLD")));
  EXPECT_CALL(editor_, RenamePhrase(TestId(1), Eq("OLD")))
      .WillOnce(Return(core::InternalError("unexpected")));
  ASSERT_TRUE(edits_.RenamePhrase(TestId(1), "NEW").IsOk());
  stack_.undo();
  EXPECT_EQ(stack_.count(), 0);
}

TEST(ProjectCommandsOnProjectTest, UndoingSortRestoresThePreviousOrder) {
  core::project::Project project =
      ProjectWith({AudioPhrase(1, "CHARLIE"), AudioPhrase(2, "ALPHA"),
                   AudioPhrase(3, "BRAVO")});
  QUndoStack stack;
  ProjectEdits edits(project, stack);
  ASSERT_TRUE(edits.SortPhrases().IsOk());
  EXPECT_EQ(project.Order(),
            (std::vector<PhraseId>{TestId(2), TestId(3), TestId(1)}));
  stack.undo();
  EXPECT_EQ(project.Order(),
            (std::vector<PhraseId>{TestId(1), TestId(2), TestId(3)}));
  stack.redo();
  EXPECT_EQ(project.Order(),
            (std::vector<PhraseId>{TestId(2), TestId(3), TestId(1)}));
}

TEST(ProjectCommandsOnProjectTest, EveryOperationUndoesToTheSameProject) {
  core::project::Project project =
      ProjectWith({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
  const core::project::Project original = project;
  QUndoStack stack;
  ProjectEdits edits(project, stack);
  ASSERT_TRUE(
      edits.InsertPhrases({{.index = 1, .phrase = AudioPhrase(3, "THREE")}})
          .IsOk());
  ASSERT_TRUE(edits.RenamePhrase(TestId(1), "first").IsOk());
  ASSERT_TRUE(edits.SetDescription(TestId(2), "the second").IsOk());
  const std::vector<PhraseId> moved = {TestId(1)};
  ASSERT_TRUE(edits.MovePhrases(moved, 3).IsOk());
  const std::vector<PhraseId> removed = {TestId(3)};
  ASSERT_TRUE(edits.RemovePhrases(removed).IsOk());
  ASSERT_TRUE(edits.SortPhrases().IsOk());
  EXPECT_EQ(stack.count(), 6);
  const core::project::Project edited = project;
  while (stack.canUndo()) {
    stack.undo();
  }
  EXPECT_EQ(project, original);
  while (stack.canRedo()) {
    stack.redo();
  }
  EXPECT_EQ(project, edited);
}

}  // namespace
}  // namespace speak5220::app
