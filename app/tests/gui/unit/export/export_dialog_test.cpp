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
            .confirm_overwrite = {},
            .set_presets = {}};
  }

  // Start(), saving presets into project_ as the main window does, and
  // keeping the names of the steps.
  ExportStart SavingStart(QStringList selected = {}) {
    ExportStart start = Start(std::move(selected));
    start.set_presets = [this](std::vector<ExportPreset> presets,
                               const QString& text) {
      steps_ << text;
      return project_.SetExports(std::move(presets)).GetStatus();
    };
    return start;
  }

  static void ChoosePreset(ExportDialog& dialog, const QString& name) {
    const int index = dialog.PresetBox()->findData(name);
    dialog.PresetBox()->setCurrentIndex(index);
    emit dialog.PresetBox()->activated(index);
  }

  static void SelectFormat(ExportDialog& dialog, const QString& key) {
    dialog.FormatBox()->setCurrentIndex(dialog.FormatBox()->findData(key));
  }

  Project project_;
  QStringList steps_;
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

TEST_F(ExportDialogTest, PresetFillsTheFields) {
  ASSERT_TRUE(project_
                  .SetExports({{.name = "words",
                                .format = "wav",
                                .scope = "selected",
                                .phrases = {"C"},
                                .audio = "original"}})
                  .IsOk());
  ExportDialog dialog(Start());
  ASSERT_EQ(dialog.PresetBox()->count(), 2);
  ChoosePreset(dialog, QStringLiteral("words"));
  const ExportPreset preset = dialog.Request().preset;
  EXPECT_EQ(preset.name, "words");
  EXPECT_EQ(preset.format, "wav");
  EXPECT_EQ(preset.scope, "selected");
  EXPECT_EQ(preset.phrases, std::vector<std::string>{"C"});
  EXPECT_EQ(preset.audio, "original");
  EXPECT_EQ(dialog.NameEdit()->text(), QStringLiteral("words"));
  EXPECT_EQ(dialog.Summary(), QStringLiteral("Writes 1 file: words_C.wav"));
}

TEST_F(ExportDialogTest, WithoutSavingPresetsAreOnlyLoaded) {
  ExportDialog dialog(Start());
  EXPECT_FALSE(dialog.SavePresetButton()->isVisibleTo(&dialog));
  EXPECT_FALSE(dialog.DeletePresetButton()->isVisibleTo(&dialog));
  EXPECT_FALSE(dialog.PresetBox()->isEnabled());
}

TEST_F(ExportDialogTest, SavesTheFieldsAsAPresetNamedAfterTheName) {
  ExportDialog dialog(SavingStart({QStringLiteral("A")}));
  SelectFormat(dialog, QStringLiteral("acorn-phrom"));
  dialog.SelectedButton()->setChecked(true);
  dialog.LayoutBox()->setCurrentIndex(
      dialog.LayoutBox()->findData(QStringLiteral("concatenated")));
  dialog.NameEdit()->setText(QStringLiteral("rom"));
  EXPECT_EQ(dialog.SavePresetButton()->text(),
            QStringLiteral("&Save as Preset"));
  dialog.SavePresetButton()->click();
  ASSERT_EQ(project_.Contents().exports.size(), 1U);
  const ExportPreset& saved = project_.Contents().exports.front();
  EXPECT_EQ(saved.name, "rom");
  EXPECT_EQ(saved.format, "acorn-phrom");
  EXPECT_EQ(saved.scope, "selected");
  EXPECT_EQ(saved.phrases, std::vector<std::string>{"A"});
  EXPECT_EQ(saved.layout, "concatenated");
  EXPECT_EQ(steps_, QStringList{QStringLiteral("Save Export Preset rom")});
  EXPECT_EQ(dialog.PresetBox()->currentData().toString(),
            QStringLiteral("rom"));
  // Saving again under the same name replaces it.
  EXPECT_EQ(dialog.SavePresetButton()->text(),
            QStringLiteral("&Update Preset"));
  dialog.AllButton()->setChecked(true);
  dialog.SavePresetButton()->click();
  ASSERT_EQ(project_.Contents().exports.size(), 1U);
  EXPECT_EQ(project_.Contents().exports.front().scope, "all");
  EXPECT_EQ(steps_.last(), QStringLiteral("Update Export Preset rom"));
}

TEST_F(ExportDialogTest, UpdatingAPresetKeepsItsUnknownKeys) {
  ASSERT_TRUE(project_
                  .SetExports({{.name = "rom",
                                .format = "acorn-phrom",
                                .unknown = {{"future", "\"yes\""}}}})
                  .IsOk());
  ExportDialog dialog(SavingStart());
  ChoosePreset(dialog, QStringLiteral("rom"));
  dialog.SavePresetButton()->click();
  ASSERT_EQ(project_.Contents().exports.size(), 1U);
  EXPECT_EQ(project_.Contents().exports.front().unknown.size(), 1U);
}

TEST_F(ExportDialogTest, APresetMustBeOneThatCanRun) {
  ExportDialog dialog(SavingStart());
  dialog.NameEdit()->setText(QStringLiteral("../up"));
  EXPECT_FALSE(dialog.SavePresetButton()->isEnabled());
}

TEST_F(ExportDialogTest, DeletesTheChosenPreset) {
  ASSERT_TRUE(project_
                  .SetExports({{.name = "rom", .format = "acorn-phrom"},
                               {.name = "words", .format = "wav"}})
                  .IsOk());
  ExportDialog dialog(SavingStart());
  EXPECT_FALSE(dialog.DeletePresetButton()->isEnabled());
  ChoosePreset(dialog, QStringLiteral("rom"));
  ASSERT_TRUE(dialog.DeletePresetButton()->isEnabled());
  dialog.DeletePresetButton()->click();
  ASSERT_EQ(project_.Contents().exports.size(), 1U);
  EXPECT_EQ(project_.Contents().exports.front().name, "words");
  EXPECT_EQ(steps_, QStringList{QStringLiteral("Delete Export Preset rom")});
  EXPECT_EQ(dialog.PresetBox()->count(), 2);
  EXPECT_FALSE(dialog.DeletePresetButton()->isEnabled());
}

}  // namespace
}  // namespace speak5220::app
