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
#include <filesystem>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/project/phrase.h"
#include "core/project/phrase_importer.h"
#include "core/project/project_store.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "core/util/version.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/project_document.h"
#include "gui/main_window/recent_files.h"
#include "gui/main_window/status_text.h"
#include "gui/phrase_editor/phrase_details.h"
#include "gui/phrase_list/phrase_list_model.h"
#include "gui/phrase_list/phrase_list_panel.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

// The window's initial size in device-independent pixels.
constexpr int kInitialWidth = 1024;
constexpr int kInitialHeight = 640;

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
                       core::ITaskRunner& runner, QWidget* parent)
    : QMainWindow(parent),
      dialogs_(&dialogs),
      recent_store_(&recent_store),
      runner_(&runner),
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

  OnProjectChanged();
  UpdateTitle();
  UpdateRecentMenu();
}

MainWindow::~MainWindow() {
  // Jobs use the importer and post back to this window.
  runner_->Wait();
  disconnect(&document_, nullptr, this, nullptr);
  // Child widgets outlive the members (QObject deletes them later), so
  // the two that use the model and the document go first; otherwise their
  // signals on the way out would reach destroyed members.
  delete panel_;
  delete details_;
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
  layout->addWidget(details_);
  layout->addStretch();
  setCentralWidget(central);

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
  details_->ShowPhrase(id.empty() ? nullptr
                                  : document_.Project().FindPhrase(id));
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
