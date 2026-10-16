/*
 * File:        export_dialog_files_test.cpp
 * Module:      export tests
 * Purpose:     Functional tests of the Export dialog against a real
 *              folder: it asks before replacing existing files, and stays
 *              open when the user declines
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QDialog>
#include <QFile>
#include <QLineEdit>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <string>

#include "core/encoder/encoder_options.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "gtest/gtest.h"
#include "gui/export/export_dialog.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using core::project::Phrase;

// An encoded phrase whose encoding is current with the default options.
Phrase CurrentPhrase(int n, const std::string& name, int cost) {
  Phrase phrase = testing::EncodedPhrase(n, name, cost);
  phrase.encoding = core::project::EncodingCache::MakeEncoding(
      phrase.audio, core::encoder::EncoderOptions{}, phrase.encoding->frames);
  return phrase;
}

class ExportDialogFilesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(directory_.isValid());
    // One PHROM: the export writes speech_1.bin, which already exists.
    QFile existing(directory_.filePath(QStringLiteral("speech_1.bin")));
    ASSERT_TRUE(existing.open(QIODevice::WriteOnly));
    existing.write("mine");
  }

  // A dialog that answers the overwrite question with `answer`, counting
  // the questions and keeping the names asked about.
  ExportStart Start(bool answer) {
    return {
        .project = &project_,
        .selected = {},
        .base = QStringLiteral("speech"),
        .directory = directory_.path(),
        .confirm_overwrite =
            [this, answer](const QString& directory, const QStringList& names) {
              ++questions_;
              asked_directory_ = directory;
              asked_names_ = names;
              return answer;
            },
        .set_presets = {}};
  }

  QTemporaryDir directory_;
  core::project::Project project_ =
      testing::ProjectWith({CurrentPhrase(1, "A", 100)});
  int questions_ = 0;
  QString asked_directory_;
  QStringList asked_names_;
};

TEST_F(ExportDialogFilesTest, DecliningKeepsTheDialogOpenForCorrections) {
  ExportDialog dialog(Start(false));
  ASSERT_TRUE(dialog.CanExport());
  EXPECT_EQ(dialog.ExistingFiles(),
            QStringList{QStringLiteral("speech_1.bin")});
  dialog.open();
  dialog.accept();
  EXPECT_EQ(questions_, 1);
  EXPECT_EQ(asked_directory_, directory_.path());
  EXPECT_EQ(asked_names_, QStringList{QStringLiteral("speech_1.bin")});
  EXPECT_TRUE(dialog.isVisible());
  EXPECT_NE(dialog.result(), QDialog::Accepted);

  // A new name writes new files, so nothing is asked.
  dialog.NameEdit()->setText(QStringLiteral("other"));
  EXPECT_TRUE(dialog.ExistingFiles().isEmpty());
  dialog.accept();
  EXPECT_EQ(questions_, 1);
  EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(ExportDialogFilesTest, AgreeingAccepts) {
  ExportDialog dialog(Start(true));
  dialog.open();
  dialog.accept();
  EXPECT_EQ(questions_, 1);
  EXPECT_EQ(dialog.result(), QDialog::Accepted);
}

TEST_F(ExportDialogFilesTest, WithNoOneToAskExistingFilesAreKept) {
  ExportStart start = Start(true);
  start.confirm_overwrite = nullptr;
  ExportDialog dialog(start);
  dialog.open();
  dialog.accept();
  EXPECT_NE(dialog.result(), QDialog::Accepted);
}

}  // namespace
}  // namespace speak5220::app
