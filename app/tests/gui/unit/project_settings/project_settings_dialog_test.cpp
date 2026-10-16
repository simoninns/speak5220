/*
 * File:        project_settings_dialog_test.cpp
 * Module:      project_settings tests
 * Purpose:     Unit tests of the Project Settings dialog: it shows the
 *              project's name, header strings and ASCII map, and collects
 *              changes only when they can be applied
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/project_settings/project_settings_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <map>
#include <string>
#include <vector>

#include "core/project/project.h"
#include "core/project/project_editor.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using core::project::PhromHeader;
using core::project::Project;
using core::project::ProjectSettings;
using testing::AudioPhrase;
using testing::ProjectWith;

class ProjectSettingsDialogTest : public ::testing::Test {
 protected:
  ProjectSettingsDialogTest() {
    project_ = ProjectWith({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
    ProjectSettings settings = project_.Settings();
    settings.name = "Numbers";
    settings.header_strings = {"(C)2026 Me", "Numbers", "1.00"};
    settings.ascii_map = {{'1', "ONE"}, {'3', "THREE"}};
    settings.phroms = {{.number = 2,
                        .header_strings = {"(C)2026 Me", "Second"},
                        .unknown = {}}};
    EXPECT_TRUE(project_.SetSettings(settings).IsOk());
  }

  Project project_;
};

TEST_F(ProjectSettingsDialogTest, ShowsTheProjectsSettings) {
  ProjectSettingsDialog dialog(project_);
  EXPECT_EQ(dialog.NameEdit()->text(), QStringLiteral("Numbers"));
  EXPECT_EQ(dialog.HeaderEdit()->toPlainText(),
            QStringLiteral("(C)2026 Me\nNumbers\n1.00"));
  EXPECT_EQ(dialog.AsciiBox('1')->currentText(), QStringLiteral("ONE"));
  EXPECT_EQ(dialog.AsciiBox('2')->currentText(), QStringLiteral("None"));
  // An entry naming no phrase is listed as such, and kept.
  EXPECT_EQ(dialog.AsciiBox('3')->currentText(),
            QStringLiteral("THREE (no phrase of this name)"));
  const ProjectSettingsChoice chosen = dialog.Chosen();
  EXPECT_EQ(chosen.identity.name, "Numbers");
  EXPECT_EQ(chosen.identity.header_strings, project_.Settings().header_strings);
  EXPECT_EQ(chosen.phrom_settings.ascii_map, project_.Settings().ascii_map);
  EXPECT_EQ(chosen.phrom_settings.phroms, project_.Settings().phroms);
  EXPECT_TRUE(dialog.OkButton()->isEnabled());
}

TEST_F(ProjectSettingsDialogTest, CollectsTheChanges) {
  ProjectSettingsDialog dialog(project_);
  dialog.NameEdit()->setText(QStringLiteral("  Words "));
  dialog.HeaderEdit()->setPlainText(QStringLiteral("(C)2026 You\nWords\n"));
  dialog.AsciiBox('1')->setCurrentIndex(0);
  dialog.AsciiBox('2')->setCurrentIndex(
      dialog.AsciiBox('2')->findData(QStringLiteral("TWO")));
  const ProjectSettingsChoice chosen = dialog.Chosen();
  EXPECT_EQ(chosen.identity.name, "Words");
  // Empty lines at the end are left out.
  EXPECT_EQ(chosen.identity.header_strings,
            (std::vector<std::string>{"(C)2026 You", "Words"}));
  const std::map<char, std::string> map = {{'2', "TWO"}, {'3', "THREE"}};
  EXPECT_EQ(chosen.phrom_settings.ascii_map, map);
}

TEST_F(ProjectSettingsDialogTest, RefusesHeaderStringsThatDoNotFit) {
  ProjectSettingsDialog dialog(project_);
  dialog.HeaderEdit()->setPlainText(
      QStringLiteral("(C)%1\nWords").arg(QString(50, QLatin1Char('x'))));
  EXPECT_FALSE(dialog.OkButton()->isEnabled());
  EXPECT_TRUE(dialog.Problem().startsWith(QStringLiteral("Header strings:")))
      << dialog.Problem().toStdString();
  dialog.accept();
  EXPECT_NE(dialog.result(), QDialog::Accepted);
  dialog.HeaderEdit()->setPlainText(QStringLiteral("(C)2026\n\nWords"));
  EXPECT_FALSE(dialog.OkButton()->isEnabled());
  dialog.HeaderEdit()->setPlainText(QStringLiteral("(C)2026\nWords"));
  EXPECT_TRUE(dialog.OkButton()->isEnabled());
}

TEST_F(ProjectSettingsDialogTest, SaysWhenTheBbcMicroWouldNotFindThePhrom) {
  ProjectSettingsDialog dialog(project_);
  EXPECT_FALSE(dialog.HeaderNote()->text().contains(QStringLiteral("(C)")));
  dialog.HeaderEdit()->setPlainText(QStringLiteral("Words"));
  EXPECT_TRUE(dialog.HeaderNote()->text().contains(QStringLiteral("(C)")));
  EXPECT_TRUE(dialog.OkButton()->isEnabled());
}

TEST_F(ProjectSettingsDialogTest, PhromsCanHaveTheirOwnStrings) {
  ProjectSettingsDialog dialog(project_);
  ASSERT_EQ(dialog.PhromSpin()->value(), 1);
  // PHROM 1 has the project's strings, with its number.
  EXPECT_FALSE(dialog.OwnStringsBox()->isChecked());
  EXPECT_FALSE(dialog.PhromStringsEdit()->isEnabled());
  EXPECT_EQ(dialog.PhromStringsEdit()->toPlainText(),
            QStringLiteral("(C)2026 Me\nNumbers 1\n1.00"));
  dialog.PhromSpin()->setValue(2);
  EXPECT_TRUE(dialog.OwnStringsBox()->isChecked());
  EXPECT_EQ(dialog.PhromStringsEdit()->toPlainText(),
            QStringLiteral("(C)2026 Me\nSecond"));

  // Giving PHROM 3 its own starts from what it has.
  dialog.PhromSpin()->setValue(3);
  dialog.OwnStringsBox()->setChecked(true);
  EXPECT_EQ(dialog.PhromStringsEdit()->toPlainText(),
            QStringLiteral("(C)2026 Me\nNumbers 3\n1.00"));
  dialog.PhromStringsEdit()->setPlainText(QStringLiteral("(C)2026 Me\nThird"));
  // PHROM 2 back to the project's.
  dialog.PhromSpin()->setValue(2);
  dialog.OwnStringsBox()->setChecked(false);

  const std::vector<PhromHeader> phroms = {
      {.number = 3, .header_strings = {"(C)2026 Me", "Third"}, .unknown = {}}};
  EXPECT_EQ(dialog.Chosen().phrom_settings.phroms, phroms);
}

TEST_F(ProjectSettingsDialogTest, RefusesAPhromsStringsThatDoNotFit) {
  ProjectSettingsDialog dialog(project_);
  dialog.PhromSpin()->setValue(2);
  dialog.PhromStringsEdit()->setPlainText(QString(60, QLatin1Char('x')));
  EXPECT_FALSE(dialog.OkButton()->isEnabled());
  EXPECT_TRUE(dialog.Problem().startsWith(QStringLiteral("PHROM 2")))
      << dialog.Problem().toStdString();
}

TEST_F(ProjectSettingsDialogTest, AcceptsSettingsTheProjectTakes) {
  ProjectSettingsDialog dialog(project_);
  dialog.NameEdit()->setText(QStringLiteral("Words"));
  dialog.accept();
  EXPECT_EQ(dialog.result(), QDialog::Accepted);
  // The dialog only collects the choice.
  EXPECT_EQ(project_.Settings().name, "Numbers");
}

}  // namespace
}  // namespace speak5220::app
