/*
 * File:        main_window.cpp
 * Module:      main_window
 * Purpose:     The application's top-level window: menus, the phrase list,
 *              the phrase details and the status bar
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/main_window.h"

#include <QAction>
#include <QActionGroup>
#include <QCloseEvent>
#include <QDir>
#include <QDockWidget>
#include <QFileInfo>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QMetaObject>
#include <QStatusBar>
#include <QString>
#include <QStringList>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/dsp/output_filter.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/phrase_importer.h"
#include "core/project/project_export.h"
#include "core/project/project_store.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "core/util/version.h"
#include "gui/encoder_panel/encoder_panel.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/encoder_panel/phrase_encoder.h"
#include "gui/export/export_dialog.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/grip_splitter.h"
#include "gui/main_window/project_document.h"
#include "gui/main_window/recent_files.h"
#include "gui/main_window/status_text.h"
#include "gui/main_window/view_preferences.h"
#include "gui/optimise/optimise_dialog.h"
#include "gui/optimise/optimise_session.h"
#include "gui/phrase_editor/phrase_details.h"
#include "gui/phrase_list/phrase_list_model.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/playback/player.h"
#include "gui/undo/project_commands.h"
#include "gui/waveform/phrase_waveforms.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

// The window's initial size in device-independent pixels.
constexpr int kInitialWidth = 1100;
constexpr int kInitialHeight = 820;
// The share of the height the waveforms start with, against the encoder
// panel's.
constexpr int kWaveformsStretch = 3;
constexpr int kEncoderStretch = 2;

// How long a progress message stays in the status bar, in milliseconds.
constexpr int kStatusMessageMs = 5000;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

std::filesystem::path ToPath(const QString& text) {
  return {text.toStdU16String()};
}

}  // namespace

MainWindow::MainWindow(IDialogs& dialogs, IRecentFilesStore& recent_store,
                       IViewPreferences& view_preferences,
                       core::ITaskRunner& runner, core::synth::ISynth& synth,
                       Player& player, const EncodingTools& tools,
                       QWidget* parent)
    : QMainWindow(parent),
      dialogs_(&dialogs),
      recent_store_(&recent_store),
      view_preferences_(&view_preferences),
      runner_(&runner),
      synth_(&synth),
      player_(&player),
      tools_(tools),
      recent_(recent_store.Load()) {
  resize(kInitialWidth, kInitialHeight);
  CreateActions();
  CreateWidgets();
  CreateMenus();

  connect(&document_, &ProjectDocument::Changed, this,
          &MainWindow::OnProjectChanged);
  connect(&document_, &ProjectDocument::ModifiedChanged, this,
          &MainWindow::UpdateTitle);
  connect(&document_, &ProjectDocument::PathChanged, this,
          &MainWindow::UpdateTitle);
  connect(&model_, &PhraseListModel::EditFailed, this,
          [this](const QString& message) {
            ShowError(QStringLiteral("Edit Phrases"), message);
          });
  connect(&model_, &PhraseListModel::FilesDropped, this,
          &MainWindow::ImportFiles);
  connect(panel_, &PhraseListPanel::SelectionChanged, this,
          &MainWindow::OnSelectionChanged);
  connect(waveforms_, &PhraseWaveforms::Failed, this, &MainWindow::ShowError);
  connect(encoder_panel_, &EncoderPanel::Failed, this, &MainWindow::ShowError);
  connect(encoder_panel_, &EncoderPanel::PreprocessingChanged, waveforms_,
          &PhraseWaveforms::SetPreprocessing);
  waveforms_->SetPreprocessedShown(view_preferences_->PreprocessedShown());
  connect(
      waveforms_, &PhraseWaveforms::PreprocessedShownChanged, this,
      [this](bool shown) { view_preferences_->SetPreprocessedShown(shown); });
  connect(encoder_panel_, &EncoderPanel::OptimiseRequested, this,
          [this](const project::PhraseId& id) { Optimise({id}); });
  connect(optimise_dialog_, &OptimiseDialog::Failed, this,
          &MainWindow::ShowError);
  encoder_panel_->Form()->SetAdvancedShown(
      view_preferences_->AdvancedSettingsShown());
  connect(encoder_panel_->Form(), &EncoderSettingsForm::AdvancedShownChanged,
          this, [this](bool shown) {
            view_preferences_->SetAdvancedSettingsShown(shown);
          });

  SetOutputFilter(view_preferences_->ChosenOutputFilter());
  SetOutputPin(view_preferences_->ChosenOutputPin());

  OnProjectChanged();
  UpdateTitle();
  UpdateRecentMenu();
}

MainWindow::~MainWindow() {
  // Optimise's search uses the runner; it stops after the candidates
  // running.
  optimise_session_->Cancel();
  tools_.optimiser_driver->Wait();
  // Jobs use the importer and post back to this window.
  runner_->Wait();
  disconnect(&document_, nullptr, this, nullptr);
  // Child widgets outlive the members (QObject deletes them later), so
  // those that use the model and the document go first; otherwise their
  // signals on the way out would reach destroyed members.
  delete panel_;
  delete details_;
  delete waveforms_;
  delete encoder_panel_;
  delete optimise_dialog_;
  delete optimise_session_;
}

void MainWindow::SetOutputFilter(core::dsp::OutputFilter filter) {
  player_->SetOutputFilter(filter);
  waveforms_->SetOutputFilter(filter);
  if (QAction* action = OutputFilterAction(filter); action != nullptr) {
    action->setChecked(true);
  }
  view_preferences_->SetOutputFilter(filter);
}

QAction* MainWindow::OutputFilterAction(core::dsp::OutputFilter filter) const {
  const std::span<const core::dsp::OutputFilterInfo> filters =
      core::dsp::OutputFilters();
  for (std::size_t i = 0; i < filters.size(); ++i) {
    if (filters[i].filter == filter && i < output_filter_actions_.size()) {
      return output_filter_actions_[i];
    }
  }
  return nullptr;
}

void MainWindow::SetOutputPin(core::synth::OutputPin pin) {
  // The audio being played was taken from the other pin.
  if (player_->State() != PlaybackState::kStopped &&
      player_->Source() != PlaybackSource::kOriginal &&
      waveforms_->Pin() != pin) {
    player_->Stop();
  }
  waveforms_->SetOutputPin(pin);
  optimise_dialog_->SetOutputPin(pin);
  if (QAction* action = OutputPinAction(pin); action != nullptr) {
    action->setChecked(true);
  }
  view_preferences_->SetOutputPin(pin);
}

QAction* MainWindow::OutputPinAction(core::synth::OutputPin pin) const {
  const std::span<const core::synth::OutputPinInfo> pins =
      core::synth::OutputPins();
  for (std::size_t i = 0; i < pins.size(); ++i) {
    if (pins[i].pin == pin && i < output_pin_actions_.size()) {
      return output_pin_actions_[i];
    }
  }
  return nullptr;
}

QString MainWindow::StatusText() const { return status_label_->text(); }

void MainWindow::CreateActions() {
  new_action_ = new QAction(QStringLiteral("&New"), this);
  new_action_->setShortcut(QKeySequence::New);
  connect(new_action_, &QAction::triggered, this, [this] { NewProject(); });

  open_action_ = new QAction(QStringLiteral("&Open..."), this);
  open_action_->setShortcut(QKeySequence::Open);
  connect(open_action_, &QAction::triggered, this, &MainWindow::OpenFromDialog);

  save_action_ = new QAction(QStringLiteral("&Save"), this);
  save_action_->setShortcut(QKeySequence::Save);
  connect(save_action_, &QAction::triggered, this, [this] { Save(); });

  save_as_action_ = new QAction(QStringLiteral("Save &As..."), this);
  save_as_action_->setShortcut(QKeySequence::SaveAs);
  connect(save_as_action_, &QAction::triggered, this, [this] { SaveAs(); });

  clear_recent_action_ = new QAction(QStringLiteral("&Clear Menu"), this);
  connect(clear_recent_action_, &QAction::triggered, this, [this] {
    recent_.Clear();
    recent_store_->Save(recent_.Paths());
    UpdateRecentMenu();
  });

  quit_action_ = new QAction(QStringLiteral("&Quit"), this);
  quit_action_->setShortcut(QKeySequence::Quit);
  quit_action_->setMenuRole(QAction::QuitRole);
  connect(quit_action_, &QAction::triggered, this, &QWidget::close);

  undo_action_ =
      document_.UndoStack().createUndoAction(this, QStringLiteral("&Undo"));
  undo_action_->setShortcut(QKeySequence::Undo);
  redo_action_ =
      document_.UndoStack().createRedoAction(this, QStringLiteral("&Redo"));
  redo_action_->setShortcut(QKeySequence::Redo);

  output_filter_group_ = new QActionGroup(this);
  for (const core::dsp::OutputFilterInfo& info : core::dsp::OutputFilters()) {
    auto* action = new QAction(ToQString(info.label), output_filter_group_);
    action->setCheckable(true);
    action->setStatusTip(
        info.filter == core::dsp::OutputFilter::kNone
            ? QStringLiteral("Hear and see the chip's DAC output unfiltered")
            : QStringLiteral("Hear and see the chip's output through the %1's "
                             "audio stage")
                  .arg(ToQString(info.label)));
    connect(action, &QAction::triggered, this,
            [this, filter = info.filter] { SetOutputFilter(filter); });
    output_filter_actions_.push_back(action);
  }

  output_pin_group_ = new QActionGroup(this);
  for (const core::synth::OutputPinInfo& info : core::synth::OutputPins()) {
    auto* action = new QAction(ToQString(info.label), output_pin_group_);
    action->setCheckable(true);
    action->setStatusTip(
        info.pin == core::synth::OutputPin::kSpeaker
            ? QStringLiteral("Hear and see the DAC's output, which clips each "
                             "code to -128 to +127")
            : QStringLiteral("Hear and see the 10-bit codes on the I/O pin, "
                             "before the DAC clips them"));
    connect(action, &QAction::triggered, this,
            [this, pin = info.pin] { SetOutputPin(pin); });
    output_pin_actions_.push_back(action);
  }

  add_action_ = new QAction(QStringLiteral("&Add Phrases..."), this);
  add_action_->setIconText(QStringLiteral("+"));
  add_action_->setToolTip(QStringLiteral("Add phrases from WAV files"));
  add_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_A));
  connect(add_action_, &QAction::triggered, this, &MainWindow::AddFromDialog);

  remove_action_ = new QAction(QStringLiteral("&Remove"), this);
  remove_action_->setIconText(QStringLiteral("−"));
  remove_action_->setToolTip(QStringLiteral("Remove the selected phrases"));
  remove_action_->setShortcut(QKeySequence::Delete);
  connect(remove_action_, &QAction::triggered, this,
          &MainWindow::RemoveSelected);

  move_up_action_ = new QAction(QStringLiteral("Move &Up"), this);
  move_up_action_->setIconText(QStringLiteral("↑"));
  move_up_action_->setToolTip(QStringLiteral("Move the selected phrases up"));
  move_up_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Up));
  connect(move_up_action_, &QAction::triggered, this,
          [this] { model_.MoveUp(panel_->SelectedRows()); });

  move_down_action_ = new QAction(QStringLiteral("Move &Down"), this);
  move_down_action_->setIconText(QStringLiteral("↓"));
  move_down_action_->setToolTip(
      QStringLiteral("Move the selected phrases down"));
  move_down_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_Down));
  connect(move_down_action_, &QAction::triggered, this,
          [this] { model_.MoveDown(panel_->SelectedRows()); });

  sort_action_ = new QAction(QStringLiteral("&Sort A-Z"), this);
  sort_action_->setIconText(QStringLiteral("A-Z"));
  sort_action_->setToolTip(QStringLiteral("Sort the phrases by name"));
  connect(sort_action_, &QAction::triggered, this, [this] { model_.Sort(); });

  re_encode_action_ = new QAction(QStringLiteral("Re-&encode"), this);
  re_encode_action_->setToolTip(
      QStringLiteral("Encode the selected phrases that are not encoded or "
                     "are stale"));
  re_encode_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
  connect(re_encode_action_, &QAction::triggered, this,
          &MainWindow::ReEncodeSelected);

  optimise_action_ = new QAction(QStringLiteral("&Optimise..."), this);
  optimise_action_->setToolTip(QStringLiteral(
      "Search the selected phrases' settings for the best quality"));
  optimise_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O));
  connect(optimise_action_, &QAction::triggered, this,
          [this] { Optimise(EncodableSelection()); });

  export_action_ = new QAction(QStringLiteral("&Export..."), this);
  export_action_->setToolTip(QStringLiteral(
      "Export PHROM images, C headers or WAV files, or run a saved preset"));
  export_action_->setShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E));
  connect(export_action_, &QAction::triggered, this,
          &MainWindow::ExportProject);

  about_action_ = new QAction(QStringLiteral("&About Speak5220"), this);
  about_action_->setMenuRole(QAction::AboutRole);
  connect(about_action_, &QAction::triggered, this, &MainWindow::ShowAbout);
}

void MainWindow::CreateMenus() {
  QMenu* file = menuBar()->addMenu(QStringLiteral("&File"));
  file->addAction(new_action_);
  file->addAction(open_action_);
  recent_menu_ = file->addMenu(QStringLiteral("Open &Recent"));
  file->addSeparator();
  file->addAction(save_action_);
  file->addAction(save_as_action_);
  file->addSeparator();
  file->addAction(quit_action_);

  QMenu* edit = menuBar()->addMenu(QStringLiteral("&Edit"));
  edit->addAction(undo_action_);
  edit->addAction(redo_action_);

  QMenu* phrase = menuBar()->addMenu(QStringLiteral("&Phrase"));
  phrase->addAction(add_action_);
  phrase->addAction(remove_action_);
  phrase->addSeparator();
  phrase->addAction(move_up_action_);
  phrase->addAction(move_down_action_);
  phrase->addAction(sort_action_);
  phrase->addSeparator();
  phrase->addAction(re_encode_action_);
  phrase->addAction(optimise_action_);

  QMenu* exports = menuBar()->addMenu(QStringLiteral("E&xport"));
  exports->addAction(export_action_);

  QMenu* audio = menuBar()->addMenu(QStringLiteral("&Audio"));
  QMenu* output_pin = audio->addMenu(QStringLiteral("Chip &Output"));
  for (QAction* action : output_pin_actions_) {
    output_pin->addAction(action);
  }
  QMenu* output_filter = audio->addMenu(QStringLiteral("Output &Filter"));
  for (QAction* action : output_filter_actions_) {
    output_filter->addAction(action);
  }

  QMenu* help = menuBar()->addMenu(QStringLiteral("&Help"));
  help->addAction(about_action_);
}

void MainWindow::CreateWidgets() {
  panel_ = new PhraseListPanel(model_,
                               {add_action_, remove_action_, move_up_action_,
                                move_down_action_, sort_action_},
                               this);
  auto* dock = new QDockWidget(QStringLiteral("Phrases"), this);
  dock->setObjectName(QStringLiteral("phrases"));
  dock->setFeatures(QDockWidget::DockWidgetMovable);
  dock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
  dock->setWidget(panel_);
  addDockWidget(Qt::LeftDockWidgetArea, dock);

  auto* central = new QWidget(this);
  auto* layout = new QVBoxLayout(central);
  details_ = new PhraseDetails(document_.Edits(), central);
  // The waveforms above the encoder panel, with a splitter between them
  // (GripSplitter, so its handle can be seen in every style).
  auto* splitter = new GripSplitter(Qt::Vertical, central);
  waveforms_ = new PhraseWaveforms(document_.Edits(), *player_, *synth_,
                                   *runner_, splitter);
  encoder_panel_ =
      new EncoderPanel(document_.Edits(), tools_, *synth_, *runner_, splitter);
  splitter->addWidget(waveforms_);
  splitter->addWidget(encoder_panel_);
  splitter->setStretchFactor(0, kWaveformsStretch);
  splitter->setStretchFactor(1, kEncoderStretch);
  splitter->setChildrenCollapsible(false);
  layout->addWidget(details_);
  layout->addWidget(splitter, 1);
  setCentralWidget(central);

  optimise_session_ =
      new OptimiseSession(document_.Edits(), tools_, *runner_, this);
  optimise_dialog_ =
      new OptimiseDialog(*optimise_session_, *player_, *synth_, *runner_, this);

  status_label_ = new QLabel(this);
  statusBar()->addPermanentWidget(status_label_);
}

bool MainWindow::MaybeSave() {
  if (!document_.IsModified()) {
    return true;
  }
  switch (dialogs_->AskSaveChanges(document_.DisplayName())) {
    case SaveChoice::kSave:
      return Save();
    case SaveChoice::kDiscard:
      return true;
    case SaveChoice::kCancel:
      return false;
  }
  return false;
}

bool MainWindow::NewProject() {
  if (!MaybeSave()) {
    return false;
  }
  ++generation_;
  document_.New();
  return true;
}

bool MainWindow::OpenProject(const QString& path) {
  if (!MaybeSave()) {
    return false;
  }
  if (const core::Status status = document_.Open(path); !status.IsOk()) {
    if (!QFileInfo::exists(path)) {
      recent_.Remove(path);
      recent_store_->Save(recent_.Paths());
      UpdateRecentMenu();
    }
    ShowError(QStringLiteral("Open Project"), ToQString(status.Message()));
    return false;
  }
  ++generation_;
  RememberRecent(document_.Path());
  return true;
}

bool MainWindow::Save() {
  if (document_.Path().isEmpty()) {
    return SaveAs();
  }
  if (const core::Status status = document_.Save(); !status.IsOk()) {
    ShowError(QStringLiteral("Save Project"), ToQString(status.Message()));
    return false;
  }
  statusBar()->showMessage(
      QStringLiteral("Saved %1").arg(document_.DisplayName()),
      kStatusMessageMs);
  return true;
}

bool MainWindow::SaveAs() {
  QString suggested = document_.Path();
  if (suggested.isEmpty()) {
    const std::string& name = document_.Project().Settings().name;
    suggested = QDir::home().filePath(
        (name.empty() ? QString::fromLatin1(ProjectDocument::kUntitledName)
                      : ToQString(name)) +
        ToQString(project::kProjectExtension));
  }
  const std::optional<QString> path = dialogs_->SaveProjectPath(suggested);
  if (!path.has_value()) {
    return false;
  }
  if (const core::Status status = document_.SaveAs(*path); !status.IsOk()) {
    ShowError(QStringLiteral("Save Project"), ToQString(status.Message()));
    return false;
  }
  RememberRecent(document_.Path());
  statusBar()->showMessage(
      QStringLiteral("Saved %1").arg(document_.DisplayName()),
      kStatusMessageMs);
  return true;
}

void MainWindow::closeEvent(QCloseEvent* event) {
  if (MaybeSave()) {
    event->accept();
  } else {
    event->ignore();
  }
}

void MainWindow::OpenFromDialog() {
  if (const std::optional<QString> path = dialogs_->OpenProjectPath()) {
    OpenProject(*path);
  }
}

void MainWindow::AddFromDialog() {
  const QStringList paths = dialogs_->ImportFilePaths();
  if (paths.isEmpty()) {
    return;
  }
  // After the last selected phrase, or at the end.
  const QList<int> rows = panel_->SelectedRows();
  ImportFiles(paths, rows.isEmpty() ? model_.PhraseCount() : rows.back() + 1);
}

void MainWindow::RemoveSelected() {
  const QList<int> rows = panel_->SelectedRows();
  if (rows.isEmpty()) {
    return;
  }
  QStringList names;
  for (const int row : rows) {
    names << model_.index(row).data().toString();
  }
  if (dialogs_->ConfirmRemove(names)) {
    model_.RemoveAt(rows);
  }
}

void MainWindow::ShowAbout() {
  QMessageBox::about(
      this, QStringLiteral("About Speak5220"),
      QStringLiteral("<p><b>Speak5220</b> %1</p>"
                     "<p>Creates TMS5220 speech data and builds PHROMs.</p>"
                     "<p>Licensed under the GNU General Public License, "
                     "version 3 or later.</p>")
          .arg(ToQString(core::Version()).toHtmlEscaped()));
}

void MainWindow::OnProjectChanged() {
  model_.SetProject(document_.Project());
  status_label_->setText(ProjectSizeText(document_.Project().Allocation()));
  OnSelectionChanged();
}

void MainWindow::OnSelectionChanged() {
  const project::PhraseId id = panel_->SingleSelectedId();
  const project::Phrase* phrase =
      id.empty() ? nullptr : document_.Project().FindPhrase(id);
  details_->ShowPhrase(phrase);
  QString note;
  if (phrase != nullptr) {
    const QList<int> rows = panel_->SelectedRows();
    switch (static_cast<EncodingState>(
        model_.index(rows.front()).data(PhraseListModel::kStateRole).toInt())) {
      case EncodingState::kStale:
        note = QStringLiteral("stale: made with other settings");
        break;
      case EncodingState::kLocked:
        note = QStringLiteral("locked");
        break;
      case EncodingState::kCurrent:
      case EncodingState::kNotEncoded:
        break;
    }
  }
  waveforms_->ShowPhrase(phrase, note);
  encoder_panel_->ShowPhrase(document_.Project(), id);
  UpdateActions();
}

void MainWindow::UpdateTitle() {
  setWindowTitle(
      QStringLiteral("%1[*] - %2")
          .arg(document_.DisplayName(), ToQString(core::kApplicationName)));
  setWindowModified(document_.IsModified());
}

void MainWindow::UpdateActions() {
  const QList<int> rows = panel_->SelectedRows();
  remove_action_->setEnabled(!rows.isEmpty());
  move_up_action_->setEnabled(model_.CanMoveUp(rows));
  move_down_action_->setEnabled(model_.CanMoveDown(rows));
  sort_action_->setEnabled(model_.PhraseCount() > 1);
  re_encode_action_->setEnabled(!StaleSelection().empty());
  optimise_action_->setEnabled(!EncodableSelection().empty());
  export_action_->setEnabled(model_.PhraseCount() > 0);
}

std::vector<project::PhraseId> MainWindow::EncodableSelection() const {
  std::vector<project::PhraseId> ids;
  for (const int row : panel_->SelectedRows()) {
    const project::PhraseId id = model_.IdAt(row);
    const project::Phrase* phrase = document_.Project().FindPhrase(id);
    if (phrase != nullptr && !phrase->locked && !phrase->audio.empty()) {
      ids.push_back(id);
    }
  }
  return ids;
}

std::vector<project::PhraseId> MainWindow::StaleSelection() const {
  std::vector<project::PhraseId> ids;
  for (const project::PhraseId& id : EncodableSelection()) {
    const std::optional<int> row = model_.RowOf(id);
    if (!row.has_value()) {
      continue;
    }
    const auto state = static_cast<EncodingState>(
        model_.index(*row).data(PhraseListModel::kStateRole).toInt());
    if (state == EncodingState::kNotEncoded || state == EncodingState::kStale) {
      ids.push_back(id);
    }
  }
  return ids;
}

void MainWindow::Optimise(const std::vector<project::PhraseId>& ids) {
  if (ids.empty() || optimise_session_->IsRunning()) {
    return;
  }
  optimise_dialog_->Prepare(document_.Project(), ids);
  optimise_dialog_->open();
}

void MainWindow::ReEncodeSelected() {
  const std::vector<project::PhraseId> ids = StaleSelection();
  if (ids.empty()) {
    return;
  }
  // What each job needs: the phrase's audio and its settings now.
  struct Input {
    project::PhraseId id;
    std::vector<float> audio;
    core::encoder::EncoderOptions options;
    project::EncoderOverrides overrides;
  };
  std::vector<Input> inputs;
  const project::Project& now = document_.Project();
  for (const project::PhraseId& id : ids) {
    const project::Phrase* phrase = now.FindPhrase(id);
    core::StatusOr<core::encoder::EncoderOptions> options =
        project::EffectiveEncoderOptions(now.Settings().default_encoder,
                                         phrase->encoder);
    if (options.IsOk()) {
      inputs.push_back({.id = id,
                        .audio = phrase->audio,
                        .options = *options,
                        .overrides = phrase->encoder});
    }
  }
  ++encodes_running_;
  statusBar()->showMessage(
      QStringLiteral("Encoding %1 phrase(s)...").arg(inputs.size()));
  runner_->Post([this, inputs = std::move(inputs), tools = tools_,
                 synth = synth_, generation = generation_] {
    auto changes = std::make_shared<SetEncodingsCommand::Changes>();
    auto failures = std::make_shared<QStringList>();
    for (const Input& input : inputs) {
      core::StatusOr<MeasuredEncoding> result =
          EncodePhrase(tools, *synth, input.audio, input.options);
      if (result.IsOk()) {
        changes->emplace_back(
            input.id,
            project::EncodingChange{.overrides = input.overrides,
                                    .encoding = std::move(result->encoding)});
      } else {
        *failures << ToQString(result.GetStatus().Message());
      }
    }
    QMetaObject::invokeMethod(
        this,
        [this, changes, failures, generation] {
          --encodes_running_;
          statusBar()->clearMessage();
          if (generation == generation_ && !changes->empty()) {
            const project::Phrase* first =
                document_.Project().FindPhrase(changes->front().first);
            const QString text =
                changes->size() == 1 && first != nullptr
                    ? QStringLiteral("Re-encode %1")
                          .arg(QString::fromStdString(first->name))
                    : QStringLiteral("Re-encode %1 Phrases")
                          .arg(changes->size());
            if (const core::Status status =
                    document_.Edits().SetEncodings(std::move(*changes), text);
                !status.IsOk()) {
              *failures << ToQString(status.Message());
            }
          }
          if (!failures->isEmpty()) {
            ShowError(QStringLiteral("Re-encode"),
                      failures->join(QLatin1Char('\n')));
          }
          emit EncodeFinished();
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::ExportProject() {
  QStringList selected;
  for (const int row : panel_->SelectedRows()) {
    selected << model_.index(row).data().toString();
  }
  const QString& path = document_.Path();
  const ExportStart start{
      .project = &document_.Project(),
      .selected = selected,
      .base = QFileInfo(document_.DisplayName()).completeBaseName(),
      .directory = !last_export_directory_.isEmpty() || path.isEmpty()
                       ? last_export_directory_
                       : QFileInfo(path).absolutePath(),
      // IDialogs::AskExport() asks the user.
      .confirm_overwrite = {}};
  const std::optional<ExportRequest> request = dialogs_->AskExport(start);
  if (!request.has_value()) {
    return;
  }
  last_export_directory_ = request->directory;
  const project::ExportPreset& preset = request->preset;
  if (const core::Status status =
          project::CheckEncodingsCurrent(document_.Project(), preset);
      !status.IsOk()) {
    ShowError(QStringLiteral("Export"), QStringLiteral("%1: re-encode it first")
                                            .arg(ToQString(status.Message())));
    return;
  }
  ++exports_running_;
  statusBar()->showMessage(QStringLiteral("Exporting..."));
  runner_->Post([this, project = document_.Project(), request = *request,
                 synth = synth_, codec = &wav_codec_] {
    core::StatusOr<std::vector<project::ExportedFile>> files =
        project::RunExport(project, request.preset, request.preset.name,
                           {.synth = synth, .wav_codec = codec});
    // Not const, so the queued lambda below moves it rather than copying
    core::Status status =
        files.IsOk()
            ? project::WriteExportedFiles(
                  std::filesystem::path(request.directory.toStdU16String()),
                  *files)
            : files.GetStatus();
    const std::size_t count = files.IsOk() ? files->size() : 0;
    QMetaObject::invokeMethod(
        this,
        [this, status, count, directory = request.directory] {
          --exports_running_;
          statusBar()->clearMessage();
          if (status.IsOk()) {
            statusBar()->showMessage(
                QStringLiteral("Exported %1 file(s) to %2")
                    .arg(count)
                    .arg(QDir::toNativeSeparators(directory)),
                kStatusMessageMs);
          } else {
            ShowError(QStringLiteral("Export"), ToQString(status.Message()));
          }
          emit ExportFinished();
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::UpdateRecentMenu() {
  recent_menu_->clear();
  for (const QString& path : recent_.Paths()) {
    QAction* action =
        recent_menu_->addAction(QDir::toNativeSeparators(path), this,
                                [this, path] { OpenProject(path); });
    action->setData(path);
  }
  recent_menu_->addSeparator();
  recent_menu_->addAction(clear_recent_action_);
  recent_menu_->setEnabled(!recent_.Paths().isEmpty());
}

void MainWindow::RememberRecent(const QString& path) {
  recent_.Add(path);
  recent_store_->Save(recent_.Paths());
  UpdateRecentMenu();
}

void MainWindow::ShowError(const QString& title, const QString& message) {
  core::Logger(core::kLogModuleGui)->warn("{}", message.toStdString());
  dialogs_->ShowError(title, message);
}

void MainWindow::ImportFiles(const QStringList& paths, int row) {
  std::vector<project::ImportRequest> requests;
  std::vector<project::PhraseId> ids;
  for (const QString& path : paths) {
    requests.push_back(
        {.path = ToPath(path), .name = std::nullopt, .normalise = false});
    ids.push_back(project::NewPhraseId());
  }
  if (!requests.empty()) {
    RunImport(std::move(requests), std::move(ids), row, false);
  }
}

void MainWindow::RunImport(std::vector<project::ImportRequest> requests,
                           std::vector<project::PhraseId> ids, int row,
                           bool asked_to_normalise) {
  ++imports_running_;
  statusBar()->showMessage(
      QStringLiteral("Importing %1 file(s)...").arg(requests.size()));
  // Plan §3.1: the import runs in the background; its results come back
  // to this thread queued (AGENTS.md §4.3.3). The names taken are those
  // of the project now, and of the files before each one.
  runner_->Post([this, requests = std::move(requests), ids = std::move(ids),
                 taken = document_.Project().PhraseNames(), row,
                 asked_to_normalise, generation = generation_]() mutable {
    auto results =
        std::make_shared<std::vector<core::StatusOr<project::ImportResult>>>();
    for (std::size_t i = 0; i < requests.size(); ++i) {
      core::StatusOr<project::ImportResult> result =
          importer_.Import(requests[i], ids[i], taken);
      if (result.IsOk()) {
        taken.insert(result->phrase.name);
      }
      results->push_back(std::move(result));
    }
    QMetaObject::invokeMethod(
        this,
        [this, requests, ids, results, row, asked_to_normalise, generation] {
          FinishImport(requests, ids, std::move(*results), row,
                       asked_to_normalise, generation);
        },
        Qt::QueuedConnection);
  });
}

void MainWindow::FinishImport(
    std::vector<project::ImportRequest> requests,
    std::vector<project::PhraseId> ids,
    std::vector<core::StatusOr<project::ImportResult>> results, int row,
    bool asked_to_normalise, int generation) {
  --imports_running_;
  statusBar()->clearMessage();
  if (generation != generation_) {
    // Another project was opened meanwhile.
    emit ImportFinished();
    return;
  }

  if (!asked_to_normalise) {
    // Plan §3.1: clipping or very quiet input is normalised only if the
    // user agrees; then the files are imported again with that request,
    // as the CLI's --normalise does.
    QStringList flagged;
    for (std::size_t i = 0; i < results.size(); ++i) {
      const core::StatusOr<project::ImportResult>& result = results[i];
      if (result.IsOk() && (result->clipped || result->quiet)) {
        flagged << QFileInfo(
                       QString::fromStdU16String(requests[i].path.u16string()))
                       .fileName();
        requests[i].normalise = true;
      }
    }
    if (!flagged.isEmpty() && dialogs_->AskNormalise(flagged)) {
      RunImport(std::move(requests), std::move(ids), row, true);
      return;
    }
  }

  QStringList failures;
  std::vector<project::PlacedPhrase> placed;
  const int start = std::clamp(
      row, 0, static_cast<int>(document_.Project().Phrases().size()));
  for (std::size_t i = 0; i < results.size(); ++i) {
    core::StatusOr<project::ImportResult>& result = results[i];
    if (!result.IsOk()) {
      failures << QStringLiteral("%1: %2").arg(
          QString::fromStdU16String(requests[i].path.u16string()),
          ToQString(result.GetStatus().Message()));
      continue;
    }
    placed.push_back({.index = start + static_cast<int>(placed.size()),
                      .phrase = std::move(result->phrase)});
  }
  std::vector<project::PhraseId> added;
  added.reserve(placed.size());
  for (const project::PlacedPhrase& phrase : placed) {
    added.push_back(phrase.phrase.id);
  }
  if (!placed.empty()) {
    if (const core::Status status =
            document_.Edits().InsertPhrases(std::move(placed));
        !status.IsOk()) {
      failures << ToQString(status.Message());
      added.clear();
    }
  }
  if (!added.empty()) {
    panel_->Select(added);
    statusBar()->showMessage(
        QStringLiteral("Added %1 phrase(s)").arg(added.size()),
        kStatusMessageMs);
  }
  if (!failures.isEmpty()) {
    ShowError(QStringLiteral("Add Phrases"), failures.join(QLatin1Char('\n')));
  }
  emit ImportFinished();
}

}  // namespace speak5220::app
