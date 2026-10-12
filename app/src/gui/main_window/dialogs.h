/*
 * File:        dialogs.h
 * Module:      main_window
 * Purpose:     The questions and messages the main window puts to the
 *              user, behind an interface so tests can answer them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_DIALOGS_H_
#define SPEAK5220_GUI_MAIN_WINDOW_DIALOGS_H_

#include <QString>
#include <QStringList>
#include <optional>

#include "gui/export/export_dialog.h"

class QWidget;

namespace speak5220::app {

// The answer to "save the changes?".
enum class SaveChoice { kSave, kDiscard, kCancel };

// Every modal question and message of the main window. Modal dialogs
// cannot be answered in an offscreen test, so the window asks through this
// interface and the tests give the answers. GUI thread only.
class IDialogs {
 public:
  virtual ~IDialogs() = default;

  // A project to open: a .s5proj file, or the project.yaml of an unpacked
  // project. Nothing if cancelled.
  virtual std::optional<QString> OpenProjectPath() = 0;
  // Where to save the project; `suggested` is the current path or name.
  // Nothing if cancelled.
  virtual std::optional<QString> SaveProjectPath(const QString& suggested) = 0;
  // WAV files to import; empty if cancelled.
  virtual QStringList ImportFilePaths() = 0;

  // The project `name` has unsaved changes: save them?
  virtual SaveChoice AskSaveChanges(const QString& name) = 0;
  // Remove these phrases?
  virtual bool ConfirmRemove(const QStringList& names) = 0;
  // These files clip or are very quiet: normalise them to -1 dBFS?
  virtual bool AskNormalise(const QStringList& files) = 0;

  // The export to run, chosen in the Export dialog (ExportDialog) from
  // `start`, which asks before replacing files that already exist.
  // Nothing if cancelled.
  virtual std::optional<ExportRequest> AskExport(const ExportStart& start) = 0;

  // Something the user asked for failed.
  virtual void ShowError(const QString& title, const QString& message) = 0;
};

// IDialogs with QFileDialog and QMessageBox, parented to a window.
class QtDialogs : public IDialogs {
 public:
  // `parent` may be set later, once the window exists.
  explicit QtDialogs(QWidget* parent = nullptr) : parent_(parent) {}

  void SetParent(QWidget* parent) { parent_ = parent; }

  std::optional<QString> OpenProjectPath() override;
  std::optional<QString> SaveProjectPath(const QString& suggested) override;
  QStringList ImportFilePaths() override;
  SaveChoice AskSaveChanges(const QString& name) override;
  bool ConfirmRemove(const QStringList& names) override;
  bool AskNormalise(const QStringList& files) override;
  std::optional<ExportRequest> AskExport(const ExportStart& start) override;
  void ShowError(const QString& title, const QString& message) override;

 private:
  // These files (names) already exist in `directory`: replace them?
  static bool ConfirmOverwrite(QWidget* parent, const QString& directory,
                               const QStringList& names);

  QWidget* parent_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_DIALOGS_H_
