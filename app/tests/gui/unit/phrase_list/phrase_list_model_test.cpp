/*
 * File:        phrase_list_model_test.cpp
 * Module:      phrase_list tests
 * Purpose:     Unit tests for the phrase list model over a mocked
 *              IProjectEditor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/phrase_list/phrase_list_model.h"

#include <QApplication>
#include <QColor>
#include <QList>
#include <QMimeData>
#include <QModelIndex>
#include <QPalette>
#include <QString>
#include <QStringList>
#include <QStyle>
#include <QUndoStack>
#include <QUrl>
#include <memory>
#include <string>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/phrase_list/phrase_list_delegate.h"
#include "gui/support/gui_mocks.h"
#include "gui/theme/colours.h"
#include "gui/undo/project_edits.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"

namespace speak5220::app {
namespace {

using core::project::Phrase;
using core::project::PhraseId;
using core::project::Placement;
using core::project::Project;
using ::testing::_;
using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::IdsAre;
using testing::LockedPhrase;
using testing::MockProjectEditor;
using testing::ProjectWith;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestId;

// An encoded phrase whose encoding matches its audio and the default
// options, so it is current.
Phrase CurrentPhrase(int n, const std::string& name, int cost) {
  Phrase phrase = EncodedPhrase(n, name, cost);
  phrase.encoding->encoder_version =
      std::string(core::encoder::kEncoderVersion);
  phrase.encoding->settings_hash = core::project::EncodingCache::SettingsHash(
      phrase.audio, core::encoder::EncoderOptions{},
      core::encoder::kEncoderVersion);
  return phrase;
}

class PhraseListModelTest : public ::testing::Test {
 protected:
  QVariant Data(int row, int role) const {
    return model_.data(model_.index(row), role);
  }

  // Four phrases: PHROM 1 holds the first two (9000 + 7000 bytes), PHROM 2
  // the third, PHROM 3 the fourth (9000 + 8000 would not fit).
  void ShowFourPhrases() {
    model_.SetProject(ProjectWith(
        {EncodedPhrase(1, "ALPHA", 9000), EncodedPhrase(2, "BRAVO", 7000),
         EncodedPhrase(3, "CHARLIE", 9000), EncodedPhrase(4, "DELTA", 8000)}));
  }

  StrictMock<MockProjectEditor> editor_;
  QUndoStack stack_;
  ProjectEdits edits_{editor_, stack_};
  PhraseListModel model_{edits_};
};

TEST_F(PhraseListModelTest, RowsAreThePhrasesInListOrder) {
  model_.SetProject(
      ProjectWith({AudioPhrase(1, "HELLO"), AudioPhrase(2, "O'CLOCK")}));
  ASSERT_EQ(model_.PhraseCount(), 2);
  EXPECT_EQ(Data(0, Qt::DisplayRole).toString(), QStringLiteral("HELLO"));
  EXPECT_EQ(Data(1, Qt::DisplayRole).toString(), QStringLiteral("O'CLOCK"));
  EXPECT_EQ(Data(1, PhraseListModel::kIdRole).toString().toStdString(),
            TestId(2));
  EXPECT_EQ(model_.IdAt(1), TestId(2));
  EXPECT_EQ(model_.RowOf(TestId(2)), 1);
  EXPECT_EQ(model_.RowOf(TestId(9)), std::nullopt);
}

TEST_F(PhraseListModelTest, DividersAppearAtTheAllocatorsBoundaries) {
  ShowFourPhrases();
  const Project project = ProjectWith(
      {EncodedPhrase(1, "ALPHA", 9000), EncodedPhrase(2, "BRAVO", 7000),
       EncodedPhrase(3, "CHARLIE", 9000), EncodedPhrase(4, "DELTA", 8000)});
  const core::project::PhromAllocation allocation = project.Allocation();
  ASSERT_EQ(allocation.PhromsUsed(), 3);
  for (int row = 0; row < model_.PhraseCount(); ++row) {
    const core::project::PhraseSlot& slot =
        allocation.phrases[static_cast<std::size_t>(row)];
    EXPECT_EQ(Data(row, PhraseListModel::kPhromRole).toInt(), slot.phrom);
    EXPECT_EQ(Data(row, PhraseListModel::kWordNumberRole).toInt(),
              slot.word_number);
  }
  EXPECT_EQ(Data(0, PhraseListModel::kPhromStartRole).toInt(), 0);
  EXPECT_EQ(Data(1, PhraseListModel::kPhromStartRole).toInt(), 0);
  EXPECT_EQ(Data(2, PhraseListModel::kPhromStartRole).toInt(), 2);
  EXPECT_EQ(Data(3, PhraseListModel::kPhromStartRole).toInt(), 3);
}

TEST_F(PhraseListModelTest, DividerSkipsPhrasesInNoPhrom) {
  // The phrase not encoded between PHROM 1 and PHROM 2 is in neither; the
  // divider is above the first phrase PHROM 2 holds.
  model_.SetProject(
      ProjectWith({EncodedPhrase(1, "ALPHA", 9000), AudioPhrase(2, "BRAVO"),
                   EncodedPhrase(3, "CHARLIE", 9000)}));
  EXPECT_EQ(Data(1, PhraseListModel::kPhromStartRole).toInt(), 0);
  EXPECT_EQ(Data(2, PhraseListModel::kPhromStartRole).toInt(), 2);
}

TEST_F(PhraseListModelTest, PhraseTooLargeForAPhromIsMarked) {
  model_.SetProject(ProjectWith(
      {EncodedPhrase(1, "SMALL", 100), EncodedPhrase(2, "HUGE", 17000)}));
  EXPECT_EQ(Data(1, PhraseListModel::kPlacementRole).toInt(),
            static_cast<int>(Placement::kTooLarge));
  EXPECT_EQ(Data(1, PhraseListModel::kSizeRole).toInt(), 17000);
  EXPECT_EQ(Data(1, PhraseListModel::kPhromRole).toInt(), 0);
  // The delegate draws its name in the theme's warning colour.
  const QPalette palette = QApplication::style()->standardPalette();
  EXPECT_EQ(PhraseListDelegate::NameColour(palette, model_.index(1)),
            ColoursFor(palette).warning);
  EXPECT_EQ(PhraseListDelegate::NameColour(palette, model_.index(0)),
            palette.color(QPalette::Text));
  EXPECT_TRUE(Data(1, Qt::ToolTipRole)
                  .toString()
                  .contains(QStringLiteral("too large")));
}

TEST_F(PhraseListModelTest, ShowsEachPhrasesEncodingState) {
  model_.SetProject(ProjectWith(
      {AudioPhrase(1, "NEW"), EncodedPhrase(2, "STALE", 100),
       CurrentPhrase(3, "CURRENT", 100), LockedPhrase(4, "LOCKED", 100)}));
  EXPECT_EQ(Data(0, PhraseListModel::kStateRole).toInt(),
            static_cast<int>(EncodingState::kNotEncoded));
  EXPECT_EQ(Data(1, PhraseListModel::kStateRole).toInt(),
            static_cast<int>(EncodingState::kStale));
  EXPECT_EQ(Data(2, PhraseListModel::kStateRole).toInt(),
            static_cast<int>(EncodingState::kCurrent));
  EXPECT_EQ(Data(3, PhraseListModel::kStateRole).toInt(),
            static_cast<int>(EncodingState::kLocked));
  EXPECT_FALSE(Data(0, PhraseListModel::kSizeRole).isValid());
  EXPECT_EQ(Data(2, PhraseListModel::kSizeRole).toInt(), 100);
}

TEST_F(PhraseListModelTest, ChangedDefaultsMakeAnEncodingStale) {
  Project project = ProjectWith({CurrentPhrase(1, "HELLO", 100)});
  model_.SetProject(project);
  ASSERT_EQ(Data(0, PhraseListModel::kStateRole).toInt(),
            static_cast<int>(EncodingState::kCurrent));
  core::project::ProjectSettings settings = project.Settings();
  settings.default_encoder.gate_db = 40.0;
  ASSERT_TRUE(project.SetSettings(settings).IsOk());
  model_.SetProject(project);
  EXPECT_EQ(Data(0, PhraseListModel::kStateRole).toInt(),
            static_cast<int>(EncodingState::kStale));
}

TEST_F(PhraseListModelTest, DelegateTextShowsSizeAndState) {
  model_.SetProject(ProjectWith(
      {AudioPhrase(1, "NEW"), EncodedPhrase(2, "STALE", 312),
       CurrentPhrase(3, "CURRENT", 100), LockedPhrase(4, "LOCKED", 50)}));
  EXPECT_EQ(PhraseListDelegate::DetailText(model_.index(0)),
            QStringLiteral("not encoded"));
  EXPECT_EQ(PhraseListDelegate::DetailText(model_.index(1)),
            QStringLiteral("312 B · stale"));
  EXPECT_EQ(PhraseListDelegate::DetailText(model_.index(2)),
            QStringLiteral("100 B"));
  EXPECT_EQ(PhraseListDelegate::DetailText(model_.index(3)),
            QStringLiteral("50 B · locked"));
}

// Phrases that differ from their saved versions, or have none, are marked;
// their place in the list does not count.
TEST_F(PhraseListModelTest, MarksPhrasesChangedSinceTheLastSave) {
  const Project saved =
      ProjectWith({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
  Phrase renamed = AudioPhrase(1, "UNO");
  model_.SetProject(
      ProjectWith({AudioPhrase(2, "TWO"), renamed, AudioPhrase(3, "NEW")}),
      &saved);
  EXPECT_FALSE(Data(0, PhraseListModel::kModifiedRole).toBool());
  EXPECT_TRUE(Data(1, PhraseListModel::kModifiedRole).toBool());
  EXPECT_TRUE(Data(2, PhraseListModel::kModifiedRole).toBool());
  EXPECT_EQ(PhraseListDelegate::NameText(model_.index(0)),
            QStringLiteral("TWO"));
  EXPECT_EQ(PhraseListDelegate::NameText(model_.index(1)),
            QStringLiteral("UNO *"));
  EXPECT_EQ(Data(1, Qt::DisplayRole).toString(), QStringLiteral("UNO"));
  EXPECT_TRUE(Data(1, Qt::ToolTipRole)
                  .toString()
                  .endsWith(QStringLiteral("changed since last saved")));

  // Without a saved project, nothing is marked.
  model_.SetProject(ProjectWith({renamed}));
  EXPECT_FALSE(Data(0, PhraseListModel::kModifiedRole).toBool());
}

TEST_F(PhraseListModelTest, MoveUpPlacesThemBeforeTheRowAbove) {
  ShowFourPhrases();
  EXPECT_CALL(
      editor_,
      MovePhrases(IdsAre(std::vector<PhraseId>{TestId(3), TestId(4)}), 1))
      .WillOnce(Return(std::vector<PhraseId>{}));
  EXPECT_TRUE(model_.MoveUp({3, 2}));
}

TEST_F(PhraseListModelTest, MoveDownPlacesThemAfterTheRowBelow) {
  ShowFourPhrases();
  EXPECT_CALL(editor_, MovePhrases(IdsAre(std::vector<PhraseId>{TestId(1)}), 2))
      .WillOnce(Return(std::vector<PhraseId>{}));
  EXPECT_TRUE(model_.MoveDown({0}));
}

TEST_F(PhraseListModelTest, CannotMovePastTheEnds) {
  ShowFourPhrases();
  EXPECT_FALSE(model_.CanMoveUp({0, 2}));
  EXPECT_TRUE(model_.CanMoveDown({0, 2}));
  EXPECT_FALSE(model_.CanMoveDown({3}));
  EXPECT_FALSE(model_.CanMoveUp({}));
  // Nothing is called: StrictMock fails on any call.
  EXPECT_TRUE(model_.MoveUp({0}));
  EXPECT_TRUE(model_.MoveDown({3}));
}

TEST_F(PhraseListModelTest, SortAndRemoveCallTheEditor) {
  ShowFourPhrases();
  EXPECT_CALL(editor_, SortPhrases()).WillOnce(Return(std::vector<PhraseId>{}));
  EXPECT_CALL(editor_, RemovePhrases(
                           IdsAre(std::vector<PhraseId>{TestId(2), TestId(4)})))
      .WillOnce(Return(std::vector<core::project::PlacedPhrase>{}));
  EXPECT_TRUE(model_.Sort());
  EXPECT_TRUE(model_.RemoveAt({3, 1}));
  EXPECT_EQ(stack_.count(), 2);
}

TEST_F(PhraseListModelTest, FailedEditIsReportedAndNotPushed) {
  ShowFourPhrases();
  EXPECT_CALL(editor_, SortPhrases())
      .WillOnce(Return(core::FailedPreconditionError(
          "cannot sort the phrases: the project would need 65 PHROMs, and "
          "its budget is 64")));
  QString reported;
  QObject::connect(&model_, &PhraseListModel::EditFailed,
                   [&reported](const QString& message) { reported = message; });
  EXPECT_FALSE(model_.Sort());
  EXPECT_TRUE(reported.startsWith(QStringLiteral("cannot sort")));
  EXPECT_EQ(stack_.count(), 0);
}

TEST_F(PhraseListModelTest, DraggingRowsMovesThem) {
  ShowFourPhrases();
  const std::unique_ptr<QMimeData> data(
      model_.mimeData({model_.index(3), model_.index(2)}));
  ASSERT_TRUE(
      model_.canDropMimeData(data.get(), Qt::MoveAction, 0, 0, QModelIndex()));
  EXPECT_CALL(
      editor_,
      MovePhrases(IdsAre(std::vector<PhraseId>{TestId(3), TestId(4)}), 0))
      .WillOnce(Return(std::vector<PhraseId>{}));
  EXPECT_TRUE(
      model_.dropMimeData(data.get(), Qt::MoveAction, 0, 0, QModelIndex()));
}

TEST_F(PhraseListModelTest, DropBelowTheListMovesToTheEnd) {
  ShowFourPhrases();
  const std::unique_ptr<QMimeData> data(model_.mimeData({model_.index(0)}));
  EXPECT_CALL(editor_, MovePhrases(IdsAre(std::vector<PhraseId>{TestId(1)}), 4))
      .WillOnce(Return(std::vector<PhraseId>{}));
  EXPECT_TRUE(
      model_.dropMimeData(data.get(), Qt::MoveAction, -1, -1, QModelIndex()));
}

TEST_F(PhraseListModelTest, DroppingRowsBesideThemselvesChangesNothing) {
  ShowFourPhrases();
  const std::unique_ptr<QMimeData> data(
      model_.mimeData({model_.index(1), model_.index(2)}));
  // Before, inside or just after the block: no call, no undo step.
  for (const int row : {1, 2, 3}) {
    EXPECT_FALSE(
        model_.dropMimeData(data.get(), Qt::MoveAction, row, 0, QModelIndex()));
  }
  EXPECT_EQ(stack_.count(), 0);
}

TEST_F(PhraseListModelTest, DroppingFilesAsksForThemToBeImported) {
  ShowFourPhrases();
  QMimeData data;
  data.setUrls({QUrl::fromLocalFile(QStringLiteral("/audio/one.wav")),
                QUrl(QStringLiteral("https://example.org/two.wav")),
                QUrl::fromLocalFile(QStringLiteral("/audio/three.wav"))});
  QStringList paths;
  int row = -1;
  QObject::connect(&model_, &PhraseListModel::FilesDropped,
                   [&](const QStringList& dropped, int at) {
                     paths = dropped;
                     row = at;
                   });
  EXPECT_TRUE(model_.dropMimeData(&data, Qt::CopyAction, 2, 0, QModelIndex()));
  EXPECT_EQ(paths, (QStringList{QStringLiteral("/audio/one.wav"),
                                QStringLiteral("/audio/three.wav")}));
  EXPECT_EQ(row, 2);
}

TEST_F(PhraseListModelTest, OnlyFilesAndOwnRowsCanBeDropped) {
  ShowFourPhrases();
  QMimeData text;
  text.setText(QStringLiteral("HELLO"));
  EXPECT_FALSE(
      model_.canDropMimeData(&text, Qt::CopyAction, 0, 0, QModelIndex()));
  QMimeData remote;
  remote.setUrls({QUrl(QStringLiteral("https://example.org/a.wav"))});
  EXPECT_FALSE(
      model_.canDropMimeData(&remote, Qt::CopyAction, 0, 0, QModelIndex()));
  EXPECT_TRUE(model_.flags(QModelIndex()).testFlag(Qt::ItemIsDropEnabled));
  EXPECT_FALSE(model_.flags(model_.index(0)).testFlag(Qt::ItemIsDropEnabled));
  EXPECT_TRUE(model_.flags(model_.index(0)).testFlag(Qt::ItemIsDragEnabled));
}

}  // namespace
}  // namespace speak5220::app
