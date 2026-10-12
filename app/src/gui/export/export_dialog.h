/*
 * File:        export_dialog.h
 * Module:      export
 * Purpose:     The Export dialog: scope, format and its options, and the
 *              files it will write
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_EXPORT_EXPORT_DIALOG_H_
#define SPEAK5220_GUI_EXPORT_EXPORT_DIALOG_H_

#include <QDialog>
#include <QString>
#include <QStringList>
#include <functional>

#include "core/project/project.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QRadioButton;
class QWidget;

namespace speak5220::app {

// What the Export dialog starts from.
struct ExportStart {
  // Not owned; must outlive the dialog.
  const core::project::Project* project = nullptr;
  // The names of the phrases selected in the list, in list order.
  QStringList selected;
  // The name files are named from, such as the project file's.
  QString base;
  // The folder offered.
  QString directory;
  // Asked, with the folder and the names, when files the export would
  // write already exist there: true to replace them. Unset, existing
  // files are never replaced.
  std::function<bool(const QString& directory, const QStringList& names)>
      confirm_overwrite;
};

// What the user asked for.
struct ExportRequest {
  // The export; its name is the base its files are named from
  // (core::project::RunExport()).
  core::project::ExportPreset preset;
  QString directory;

  friend bool operator==(const ExportRequest& a,
                         const ExportRequest& b) = default;
};

// Plan §6: chooses an export. The scope is every phrase or the selected
// ones; the format's own options appear with it (the PHROM layout, the
// header licence, the WAV audio). Below, the files it will write, or why it
// cannot: a phrase not encoded with its current settings, one too large for a
// PHROM (plan §4.1), a name that cannot name files
// (core::project::ExportFileNames(), CheckEncodingsCurrent()). Export is
// enabled only for an export that can run, into a chosen folder. If any of its
// files already exist there, Export asks first (ExportStart::confirm_overwrite)
// and, if the user declines, the dialog stays open so the name or folder can be
// changed. GUI thread only.
class ExportDialog : public QDialog {
  Q_OBJECT

 public:
  explicit ExportDialog(const ExportStart& start, QWidget* parent = nullptr);

  // The export the fields describe.
  ExportRequest Request() const;
  // The files the export will write, or why it cannot run.
  QString Summary() const;
  // The names of the export's files that already exist in its folder.
  QStringList ExistingFiles() const;

  // Closes the dialog as accepted, unless the export cannot run, or
  // existing files would be replaced and the user does not agree.
  void accept() override;
  bool CanExport() const;

  QComboBox* FormatBox() const { return format_box_; }
  QRadioButton* AllButton() const { return all_button_; }
  QRadioButton* SelectedButton() const { return selected_button_; }
  QComboBox* LayoutBox() const { return layout_box_; }
  QLineEdit* LicenceEdit() const { return licence_edit_; }
  QComboBox* AudioBox() const { return audio_box_; }
  QLineEdit* NameEdit() const { return name_edit_; }
  QLineEdit* DirectoryEdit() const { return directory_edit_; }
  QPushButton* ExportButton() const { return export_button_; }

 private:
  void Update();
  void Browse();

  const core::project::Project* project_;
  std::function<bool(const QString&, const QStringList&)> confirm_overwrite_;
  // The phrases the "selected" scope exports.
  const QStringList scope_phrases_;

  QComboBox* format_box_ = nullptr;
  QRadioButton* all_button_ = nullptr;
  QRadioButton* selected_button_ = nullptr;
  QLabel* layout_label_ = nullptr;
  QComboBox* layout_box_ = nullptr;
  QLabel* licence_label_ = nullptr;
  QLineEdit* licence_edit_ = nullptr;
  QLabel* audio_label_ = nullptr;
  QComboBox* audio_box_ = nullptr;
  QLineEdit* name_edit_ = nullptr;
  QLineEdit* directory_edit_ = nullptr;
  QLabel* summary_label_ = nullptr;
  QPushButton* export_button_ = nullptr;
  bool can_export_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_EXPORT_EXPORT_DIALOG_H_
