/*
 * File:        project_settings_dialog.h
 * Module:      project_settings
 * Purpose:     The Project Settings dialog: the project's name, its PHROMs'
 *              header strings and its ASCII map
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PROJECT_SETTINGS_PROJECT_SETTINGS_DIALOG_H_
#define SPEAK5220_GUI_PROJECT_SETTINGS_PROJECT_SETTINGS_DIALOG_H_

#include <QDialog>
#include <QString>
#include <map>
#include <string>
#include <vector>

#include "core/project/project.h"
#include "core/project/project_editor.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QScrollArea;
class QSpinBox;
class QTabWidget;

namespace speak5220::app {

// What the Project Settings dialog sets. A plain value type.
struct ProjectSettingsChoice {
  core::project::ProjectIdentity identity;
  core::project::PhromSettings phrom_settings;

  friend bool operator==(const ProjectSettingsChoice& a,
                         const ProjectSettingsChoice& b) = default;
};

// The Project Settings dialog, in three tabs.
// - General: the project's name, and the header strings every PHROM
//   starts from, one per line, with the bytes of the header they take.
// - PHROMs: for the PHROM of the number chosen, whether it has header
//   strings of its own and what they are; without, the strings it gets
//   from the project, greyed out.
// - ASCII Map: the phrase each printable character speaks, or none. An
//   entry that names no phrase of the project (one renamed since) is
//   listed as such, and kept unless changed.
// OK is enabled only for header strings that fit (core::project::
// ValidateProjectHeaderStrings(), packaging::ValidateHeaderStrings()); it
// then tries the settings on a copy of the project, and stays open with
// the reason if they cannot be applied (the budget). The dialog only
// collects the choice (Chosen()); whoever opened it applies it. GUI thread
// only.
class ProjectSettingsDialog : public QDialog {
  Q_OBJECT

 public:
  // `project` must outlive the dialog.
  explicit ProjectSettingsDialog(const core::project::Project& project,
                                 QWidget* parent = nullptr);

  // The settings as the dialog shows them.
  ProjectSettingsChoice Chosen() const;
  // Why the settings cannot be applied, or empty.
  QString Problem() const;

  // Closes the dialog as accepted, unless the settings cannot be applied.
  void accept() override;

  QTabWidget* Tabs() const { return tabs_; }
  QLineEdit* NameEdit() const { return name_edit_; }
  QPlainTextEdit* HeaderEdit() const { return header_edit_; }
  QLabel* HeaderNote() const { return header_note_; }
  QSpinBox* PhromSpin() const { return phrom_spin_; }
  QCheckBox* OwnStringsBox() const { return own_box_; }
  QPlainTextEdit* PhromStringsEdit() const { return phrom_edit_; }
  // The phrase chooser of the printable ASCII `character`.
  QComboBox* AsciiBox(char character) const;
  QPushButton* OkButton() const { return ok_button_; }

 private:
  QWidget* MakeGeneralTab();
  QWidget* MakePhromsTab();
  QWidget* MakeAsciiTab();
  // Shows the PHROM of the number chosen.
  void ShowPhrom();
  // Keeps what the PHROMs tab shows for its PHROM.
  void OnPhromEdited();
  // Checks the settings, and says why they cannot be applied.
  void Update();

  const core::project::Project* project_;
  // The PHROMs with header strings of their own, by number, and the keys
  // of each that this version does not know.
  std::map<int, std::vector<std::string>> own_strings_;
  std::map<int, core::project::UnknownKeys> unknown_;
  bool showing_phrom_ = false;

  QTabWidget* tabs_;
  QLineEdit* name_edit_ = nullptr;
  QPlainTextEdit* header_edit_ = nullptr;
  QLabel* header_note_ = nullptr;
  QSpinBox* phrom_spin_ = nullptr;
  QCheckBox* own_box_ = nullptr;
  QPlainTextEdit* phrom_edit_ = nullptr;
  QLabel* phrom_note_ = nullptr;
  QLabel* own_list_ = nullptr;
  std::vector<QComboBox*> ascii_boxes_;
  QLabel* problem_label_ = nullptr;
  QPushButton* ok_button_ = nullptr;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PROJECT_SETTINGS_PROJECT_SETTINGS_DIALOG_H_
