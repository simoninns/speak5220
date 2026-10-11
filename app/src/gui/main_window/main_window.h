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
#include "core/dsp/output_filter.h"
#include "core/dsp/resampler.h"
#include "core/project/phrase.h"
#include "core/project/phrase_importer.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/file_digest.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/encoder_panel/phrase_encoder.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/project_document.h"
#include "gui/main_window/recent_files.h"
#include "gui/main_window/view_preferences.h"
#include "gui/phrase_list/phrase_list_model.h"
#include "gui/playback/player.h"

class QAction;
class QActionGroup;
class QCloseEvent;
class QLabel;
class QMenu;

namespace speak5220::app {

class EncoderPanel;
class OptimiseDialog;
class OptimiseSession;
class PhraseDetails;
class PhraseListPanel;
class PhraseWaveforms;

// Plan §5: the top-level window. The phrase list is docked on the left;
// on the right are the selected phrase's details, its waveforms
// (PhraseWaveforms) and, below a splitter, its encoder panel
// (EncoderPanel); the status bar shows the project's size in KB and
// PHROMs. Menus: File (new, open, recent, save, save as, quit), Edit
// (undo, redo), Phrase (add, remove, move up and down, sort A-Z,
// re-encode, optimise), Help.
//
// The title is the project's file name, marked while it has unsaved
// changes; replacing or closing a modified project asks first. Questions
// go through IDialogs, the recent-file list through IRecentFilesStore,
// what the user chose to see and hear (the advanced encoder settings, the
// output filter of the Audio menu) through IViewPreferences;
// imports, encoding and the rendering of encoded audio (through the
// ISynth) run as background jobs on the ITaskRunner, their results coming
// back to the GUI thread queued; the waveforms play through the Player;
// encoding and Optimise use the EncodingTools (OptimiseSession,
// OptimiseDialog). All of them must outlive the window. GUI thread only.
class MainWindow : public QMainWindow {
  Q_OBJECT

 public:
  MainWindow(IDialogs& dialogs, IRecentFilesStore& recent_store,
             IViewPreferences& view_preferences, core::ITaskRunner& runner,
             core::synth::ISynth& synth, Player& player,
             const EncodingTools& tools, QWidget* parent = nullptr);
  // Waits for background jobs still running.
  ~MainWindow() override;
  MainWindow(const MainWindow&) = delete;
  MainWindow& operator=(const MainWindow&) = delete;

  ProjectDocument& Document() { return document_; }
  PhraseListModel& Model() { return model_; }
  PhraseListPanel* Panel() const { return panel_; }
  PhraseDetails* Details() const { return details_; }
  PhraseWaveforms* Waveforms() const { return waveforms_; }
  EncoderPanel* Encoder() const { return encoder_panel_; }
  OptimiseSession* Optimisation() const { return optimise_session_; }
  OptimiseDialog* OptimiseWindow() const { return optimise_dialog_; }
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

  // Opens the Optimise window for the phrases with `ids`.
  void Optimise(const std::vector<core::project::PhraseId>& ids);
  // Re-encodes the selected phrases that are not encoded or are stale,
  // with their settings, in the background, as one undoable step.
  void ReEncodeSelected();
  // Batch re-encodings started and not yet finished.
  int EncodesRunning() const { return encodes_running_; }

  // Hears and shows the chip's output through `filter` (Audio > Output
  // Filter): the encoded waveform, and the encoded audio played here and
  // in the Optimise window. Remembered between sessions.
  void SetOutputFilter(core::dsp::OutputFilter filter);
  // The Audio > Output Filter menu's item for `filter`.
  QAction* OutputFilterAction(core::dsp::OutputFilter filter) const;
  // Takes the encoded audio from the chip's `pin` (Audio > Chip Output):
  // SPEAKER, through the DAC that clips each code to 8 bits, or I/O's
  // 10-bit codes. Shown and played here and in the Optimise window; stops
  // encoded audio playing. Remembered between sessions.
  void SetOutputPin(core::synth::OutputPin pin);
  // The Audio > Chip Output menu's item for `pin`.
  QAction* OutputPinAction(core::synth::OutputPin pin) const;

 signals:
  // An import finished, successfully or not.
  void ImportFinished();
  // A batch re-encoding finished.
  void EncodeFinished();

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

  // The ids of the selected phrases that can be encoded (unlocked, with
  // audio).
  std::vector<core::project::PhraseId> EncodableSelection() const;
  // Those of them that are not encoded, or stale.
  std::vector<core::project::PhraseId> StaleSelection() const;

  IDialogs* dialogs_;
  IRecentFilesStore* recent_store_;
  IViewPreferences* view_preferences_;
  core::ITaskRunner* runner_;
  core::synth::ISynth* synth_;
  Player* player_;
  EncodingTools tools_;
  RecentFiles recent_;

  // The importer's collaborators are stateless, so jobs can share them.
  core::audio::SndfileWavReader wav_reader_;
  core::dsp::SincResampler resampler_;
  core::FileSha256Digest digest_;
  core::project::PhraseImporter importer_{wav_reader_, resampler_, digest_};
  int imports_running_ = 0;
  int encodes_running_ = 0;
  // Counts the projects opened, so an import finishing after another
  // project was opened is dropped.
  int generation_ = 0;

  ProjectDocument document_;
  PhraseListModel model_{document_.Edits()};
  PhraseListPanel* panel_ = nullptr;
  PhraseDetails* details_ = nullptr;
  PhraseWaveforms* waveforms_ = nullptr;
  EncoderPanel* encoder_panel_ = nullptr;
  OptimiseSession* optimise_session_ = nullptr;
  OptimiseDialog* optimise_dialog_ = nullptr;
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
  // One per core::dsp::OutputFilters(), in its order, in an exclusive
  // group.
  QActionGroup* output_filter_group_ = nullptr;
  std::vector<QAction*> output_filter_actions_;
  // One per core::synth::OutputPins(), in its order, in an exclusive group.
  QActionGroup* output_pin_group_ = nullptr;
  std::vector<QAction*> output_pin_actions_;
  QAction* add_action_ = nullptr;
  QAction* remove_action_ = nullptr;
  QAction* move_up_action_ = nullptr;
  QAction* move_down_action_ = nullptr;
  QAction* sort_action_ = nullptr;
  QAction* re_encode_action_ = nullptr;
  QAction* optimise_action_ = nullptr;
  QAction* about_action_ = nullptr;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_MAIN_WINDOW_H_
