/*
 * File:        export_dialog_test.cpp
 * Module:      export tests
 * Purpose:     Unit tests of the Export dialog: its fields map to export
 *              presets and back, and it says what it will write or why it
 *              cannot
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/export/export_dialog.h"

#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QRadioButton>
#include <QString>
#include <QStringList>
#include <string>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using core::project::ExportPreset;
using core::project::Phrase;
using core::project::Project;
using testing::AudioPhrase;
using testing::LockedPhrase;
using testing::ProjectWith;

// An encoded phrase whose encoding is current with the default options.
Phrase CurrentPhrase(int n, const std::string& name, int cost) {
  Phrase phrase = testing::EncodedPhrase(n, name, cost);
  phrase.encoding = core::project::EncodingCache::MakeEncoding(
      phrase.audio, core::encoder::EncoderOptions{}, phrase.encoding->frames);
  return phrase;
}

class ExportDialogTest : public ::testing::Test {
 protected:
  ExportDialogTest() {
    // A and B fill one PHROM between them; C needs a second.
    project_ =
        ProjectWith({CurrentPhrase(1, "A", 9000), LockedPhrase(2, "B", 6000),
                     CurrentPhrase(3, "C", 3000)});
  }

  ExportStart Start(QStringList selected = {}) const {
    return {.project = &project_,
            .selected = std::move(selected),
            .base = QStringLiteral("speech"),
            .directory = QStringLiteral("/out"),
            .confirm_overwrite = {}};
  }

  static void SelectFormat(ExportDialog& dialog, const QString& key) {
    dialog.FormatBox()->setCurrentIndex(dialog.FormatBox()->findData(key));
  }

  Project project_;
};

TEST_F(ExportDialogTest, StartsAsAPhromExportOfEveryPhrase) {
  const ExportDialog dialog(Start());
  const ExportRequest request = dialog.Request();
  EXPECT_EQ(request.preset, (ExportPreset{.name = "speech",
                                          .format = "acorn-phrom",
                                          .scope = "all",
                                          .layout = "separate"}));
  EXPECT_EQ(request.directory, QStringLiteral("/out"));
  EXPECT_TRUE(dialog.CanExport());
  EXPECT_EQ(dialog.Summary(),
            QStringLiteral("Writes 2 files: speech_1.bin, speech_2.bin"));
}

TEST_F(ExportDialogTest, EachFormatTakesOnlyItsOwnOptions) {
  ExportDialog dialog(Start());
  dialog.LayoutBox()->setCurrentIndex(
      dialog.LayoutBox()->findData(QStringLiteral("concatenated")));
  dialog.LicenceEdit()->setText(QStringLiteral(" MIT "));
  dialog.AudioBox()->setCurrentIndex(
      dialog.AudioBox()->findData(QStringLiteral("original")));
  EXPECT_EQ(dialog.Request().preset.layout, "concatenated");
  EXPECT_EQ(dialog.Request().preset.licence, "");
  EXPECT_FALSE(dialog.LicenceEdit()->isVisibleTo(&dialog));

  SelectFormat(dialog, QStringLiteral("c-header-phrom"));
  EXPECT_EQ(dialog.Request().preset, (ExportPreset{.name = "speech",
                                                   .format = "c-header-phrom",
                                                   .scope = "all",
                                                   .licence = "MIT"}));
  EXPECT_TRUE(dialog.LicenceEdit()->isVisibleTo(&dialog));
  EXPECT_FALSE(dialog.LayoutBox()->isVisibleTo(&dialog));
  EXPECT_EQ(dialog.Summary(), QStringLiteral("Writes 1 file: speech.h"));

  SelectFormat(dialog, QStringLiteral("wav"));
  EXPECT_EQ(dialog.Request().preset, (ExportPreset{.name = "speech",
                                                   .format = "wav",
                                                   .scope = "all",
                                                   .audio = "original"}));
  EXPECT_TRUE(dialog.AudioBox()->isVisibleTo(&dialog));
}

TEST_F(ExportDialogTest, BbcDiscTakesNoOptions) {
  ExportDialog dialog(Start());
  SelectFormat(dialog, QStringLiteral("bbc-dsd"));
  EXPECT_EQ(
      dialog.Request().preset,
      (ExportPreset{.name = "speech", .format = "bbc-dsd", .scope = "all"}));
  EXPECT_FALSE(dialog.LayoutBox()->isVisibleTo(&dialog));
  EXPECT_FALSE(dialog.LicenceEdit()->isVisibleTo(&dialog));
  EXPECT_FALSE(dialog.AudioBox()->isVisibleTo(&dialog));
  EXPECT_TRUE(dialog.CanExport());
  EXPECT_EQ(dialog.Summary(), QStringLiteral("Writes 1 file: speech.dsd"));
}

TEST_F(ExportDialogTest, BbcDiscSaysWhenThereAreTooManyPhroms) {
  std::vector<Phrase> phrases;
  for (int i = 1; i <= 9; ++i) {
    phrases.push_back(LockedPhrase(i, "P" + std::to_string(i), 9000));
  }
  project_ = ProjectWith(std::move(phrases));
  ExportDialog dialog(Start());
  SelectFormat(dialog, QStringLiteral("bbc-dsd"));
  EXPECT_FALSE(dialog.CanExport());
  EXPECT_TRUE(dialog.Summary().contains(QStringLiteral("at most 8")));
}

TEST_F(ExportDialogTest, SelectedScopeExportsTheSelection) {
  ExportDialog dialog(Start({QStringLiteral("A"), QStringLiteral("C")}));
  EXPECT_EQ(dialog.SelectedButton()->text(),
            QStringLiteral("Selected phrases (2)"));
  dialog.SelectedButton()->setChecked(true);
  const ExportPreset preset = dialog.Request().preset;
  EXPECT_EQ(preset.scope, "selected");
  EXPECT_EQ(preset.phrases, (std::vector<std::string>{"A", "C"}));
  // Together A and C fit one PHROM.
  EXPECT_EQ(dialog.Summary(), QStringLiteral("Writes 1 file: speech_1.bin"));
}

TEST_F(ExportDialogTest, SelectedScopeNeedsASelection) {
  const ExportDialog dialog(Start());
  EXPECT_FALSE(dialog.SelectedButton()->isEnabled());
  EXPECT_TRUE(dialog.AllButton()->isChecked());
}

TEST_F(ExportDialogTest, ExportNeedsAFolder) {
  ExportDialog dialog(Start());
  dialog.DirectoryEdit()->clear();
  EXPECT_FALSE(dialog.CanExport());
  EXPECT_FALSE(dialog.ExportButton()->isEnabled());
  EXPECT_TRUE(dialog.Summary().contains(QStringLiteral("Choose a folder")));
}

TEST_F(ExportDialogTest, SaysWhyAnExportCannotRun) {
  ExportDialog dialog(Start());
  dialog.NameEdit()->setText(QStringLiteral("../up"));
  EXPECT_FALSE(dialog.CanExport());
  EXPECT_TRUE(dialog.Summary().startsWith(QStringLiteral("Cannot export:")));

  dialog.NameEdit()->setText(QStringLiteral("speech"));
  ASSERT_TRUE(dialog.CanExport());
  project_ = ProjectWith({AudioPhrase(1, "NEW")});
  ExportDialog stale(Start());
  EXPECT_FALSE(stale.CanExport());
  EXPECT_TRUE(stale.Summary().contains(QStringLiteral("NEW")));
  // The original audio needs no encoding.
  SelectFormat(stale, QStringLiteral("wav"));
  stale.AudioBox()->setCurrentIndex(
      stale.AudioBox()->findData(QStringLiteral("original")));
  EXPECT_TRUE(stale.CanExport());
  EXPECT_EQ(stale.Summary(), QStringLiteral("Writes 1 file: speech_NEW.wav"));
}

}  // namespace
}  // namespace speak5220::app
