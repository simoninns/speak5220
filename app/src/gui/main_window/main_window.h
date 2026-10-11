/*
 * File:        main_window.h
 * Module:      main_window
 * Purpose:     The application's top-level window: menus, the phrase list,
 *              the phrase details and the status bar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_MAIN_WINDOW_H_
#define SPEAK5220_GUI_MAIN_WINDOW_MAIN_WINDOW_H_

#include <QMainWindow>
#include <QString>
#include <QStringList>
#include <memory>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/dsp/resampler.h"
#include "core/project/phrase.h"
#include "core/project/phrase_importer.h"
#include "core/util/file_digest.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/project_document.h"
#include "gui/main_window/recent_files.h"
#include "gui/phrase_list/phrase_list_model.h"

class QAction;
class QCloseEvent;
class QLabel;
class QMenu;

namespace speak5220::app {

class PhraseDetails;
class PhraseListPanel;

// Plan §5: the top-level window. The phrase list is docked on the left,
// the selected phrase's details are on the right, and the status bar
// shows the project's size in KB and PHROMs. Menus: File (new, open,
// recent, save, save as, quit), Edit (undo, redo), Phrase (add, remove,
// move up and down, sort A-Z), Help.
//
// The title is the project's file name, marked while it has unsaved
// changes; replacing or closing a modified project asks first. Questions
// go through IDialogs, the recent-file list through IRecentFilesStore, and
// imports run as background jobs on the ITaskRunner, their results coming
// back to the GUI thread queued. All three must outlive the window.
// GUI thread only.
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(IDialogs& dialogs, IRecentFilesStore& recent_store,
             core::ITaskRunner& runner, QWidget* parent = nullptr);
  // Waits for imports still running.
  ~MainWindow() override;
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  ProjectDocument& Document() { return document_; }
  PhraseListModel& Model() { return model_; }
  PhraseListPanel* Panel() const { return panel_; }
  PhraseDetails* Details() const { return details_; }
  // The status bar's project summary (ProjectSizeText()).
  QString StatusText() const;
  QStringList RecentPaths() const { return recent_.Paths(); }

  // The File menu's commands. Each asks to save a modified project before
  // replacing it, and reports failures through IDialogs. They return
  // whether they did what was asked.
  bool NewProject();
  bool OpenProject(const QString& path);
  bool Save();
  bool SaveAs();

  // Imports WAV files as phrases before `row` (clamped to the list), in
  // the background. Files that clip or are very quiet are imported again
  // normalised if the user agrees (plan §3.1). The phrases are added as
  // one undoable step; files that fail are reported.
  void ImportFiles(const QStringList& paths, int row);
  // Imports started and not yet finished.
  int ImportsRunning() const { return imports_running_; }

 signals:
  // An import finished, successfully or not.
  void ImportFinished();

 protected:
  void closeEvent(QCloseEvent* event) override;

 private:
  void CreateActions();
  void CreateMenus();
  void CreateWidgets();

  // Asks whether to save a modified project; false if the user cancelled
  // or the save failed.
  bool MaybeSave();
  void OpenFromDialog();
  void AddFromDialog();
  void RemoveSelected();
  void ShowAbout();

  void OnProjectChanged();
  void OnSelectionChanged();
  void UpdateTitle();
  void UpdateActions();
  void UpdateRecentMenu();
  void RememberRecent(const QString& path);
  void ShowError(const QString& title, const QString& message);

  void RunImport(std::vector<core::project::ImportRequest> requests,
                 std::vector<core::project::PhraseId> ids, int row,
                 bool asked_to_normalise);
  void FinishImport(
      std::vector<core::project::ImportRequest> requests,
      std::vector<core::project::PhraseId> ids,
      std::vector<core::StatusOr<core::project::ImportResult>> results, int row,
      bool asked_to_normalise, int generation);

  IDialogs* dialogs_;
  IRecentFilesStore* recent_store_;
  core::ITaskRunner* runner_;
  RecentFiles recent_;

  // The importer's collaborators are stateless, so jobs can share them.
  core::audio::SndfileWavReader wav_reader_;
  core::dsp::SincResampler resampler_;
  core::FileSha256Digest digest_;
  core::project::PhraseImporter importer_{wav_reader_, resampler_, digest_};
  int imports_running_ = 0;
  // Counts the projects opened, so an import finishing after another
  // project was opened is dropped.
  int generation_ = 0;

  ProjectDocument document_;
  PhraseListModel model_{document_.Edits()};
  PhraseListPanel* panel_ = nullptr;
  PhraseDetails* details_ = nullptr;
  QLabel* status_label_ = nullptr;
  QMenu* recent_menu_ = nullptr;

  QAction* new_action_ = nullptr;
  QAction* open_action_ = nullptr;
  QAction* save_action_ = nullptr;
  QAction* save_as_action_ = nullptr;
  QAction* clear_recent_action_ = nullptr;
  QAction* quit_action_ = nullptr;
  QAction* undo_action_ = nullptr;
  QAction* redo_action_ = nullptr;
  QAction* add_action_ = nullptr;
  QAction* remove_action_ = nullptr;
  QAction* move_up_action_ = nullptr;
  QAction* move_down_action_ = nullptr;
  QAction* sort_action_ = nullptr;
  QAction* about_action_ = nullptr;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_MAIN_WINDOW_H_
