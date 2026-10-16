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
#include <string>
#include <vector>

#include "core/project/project.h"
#include "core/util/status.h"

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
  // Replaces the project's export presets with `presets`, as one undoable
  // step named `text`, and says whether it could. Unset, presets can be
  // loaded but not saved or deleted.
  std::function<core::Status(std::vector<core::project::ExportPreset> presets,
                             const QString& text)>
      set_presets;
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

// Chooses an export. A preset of the project fills the fields; the
// fields can be saved as a preset named after the Name field (replacing
// one of that name), and the chosen preset deleted
// (ExportStart::set_presets). The scope is every phrase or the selected
// ones (a preset's own, once one is loaded); the format's own options
// appear with it (the PHROM layout, the
// header licence, the WAV audio). Below, the files it will write, or why it
// cannot: a phrase not encoded with its current settings, one too large for a
// PHROM, a name that cannot name files
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

  QComboBox* PresetBox() const { return preset_box_; }
  QPushButton* SavePresetButton() const { return save_preset_button_; }
  QPushButton* DeletePresetButton() const { return delete_preset_button_; }
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
  // Lists the project's presets, choosing the one called `chosen`.
  void FillPresets(const QString& chosen);
  // Fills the fields from the preset chosen.
  void LoadPreset();
  // Saves the fields as the preset named in the Name field.
  void SavePreset();
  // Deletes the preset chosen.
  void DeletePreset();
  // The index of the project's preset called `name`, or -1.
  int PresetIndex(const std::string& name) const;

  const core::project::Project* project_;
  std::function<bool(const QString&, const QStringList&)> confirm_overwrite_;
  std::function<core::Status(std::vector<core::project::ExportPreset>,
                             const QString&)>
      set_presets_;
  // The phrases the "selected" scope exports: those selected in the list,
  // or a loaded preset's.
  QStringList scope_phrases_;

  QComboBox* preset_box_ = nullptr;
  QPushButton* save_preset_button_ = nullptr;
  QPushButton* delete_preset_button_ = nullptr;
  QLabel* preset_note_ = nullptr;
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
  bool can_save_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_EXPORT_EXPORT_DIALOG_H_
