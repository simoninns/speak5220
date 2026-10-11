/*
 * File:        project_document.h
 * Module:      main_window
 * Purpose:     The open project, its undo stack and its file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_PROJECT_DOCUMENT_H_
#define SPEAK5220_GUI_MAIN_WINDOW_PROJECT_DOCUMENT_H_

#include <QObject>
#include <QString>
#include <QUndoStack>

#include "core/project/project.h"
#include "core/util/status.h"
#include "gui/undo/project_edits.h"

namespace speak5220::app {

// The project the main window shows: the core Project, the undo stack its
// edits go onto (ProjectEdits), and the file it was opened from or saved
// to. It is modified when the undo stack is away from its clean state.
// Opening and saving use the core's stores and serializer, so the GUI and
// the CLI read and write the same files. GUI thread only.
class ProjectDocument : public QObject {
  Q_OBJECT

 public:
  // The display name of a project not yet saved.
  static constexpr const char* kUntitledName = "Untitled";

  explicit ProjectDocument(QObject* parent = nullptr);
  // QUndoStack clears itself as it is destroyed, which would signal a
  // change; the connections are cut first.
  ~ProjectDocument() override;
  ProjectDocument(const ProjectDocument&) = delete;
  ProjectDocument& operator=(const ProjectDocument&) = delete;

  const core::project::Project& Project() const { return project_; }
  ProjectEdits& Edits() { return edits_; }
  QUndoStack& UndoStack() { return undo_stack_; }

  // The .s5proj file or project directory; empty until saved or opened.
  const QString& Path() const { return path_; }
  bool IsModified() const { return !undo_stack_.isClean(); }
  // The file or directory name, or kUntitledName.
  QString DisplayName() const;

  // Replaces the project with an empty one, not yet saved.
  void New();
  // Opens a .s5proj file or a project directory; the project.yaml of a
  // project directory opens that directory. On failure the current
  // project stays open.
  core::Status Open(const QString& path);
  // Saves to Path(), which must be set, in the form it has (file or
  // directory).
  core::Status Save();
  // Saves as a .s5proj file at `path` (the extension is added if missing)
  // and makes that the project's path. A project with no name takes the
  // file's name without the extension.
  core::Status SaveAs(const QString& path);

 signals:
  // The project's contents changed: an edit, an undo or redo, or another
  // project.
  void Changed();
  void ModifiedChanged(bool modified);
  void PathChanged();

 private:
  core::Status SaveTo(const core::project::Project& project,
                      const QString& path);
  void Replace(core::project::Project project, const QString& path);

  core::project::Project project_;
  QUndoStack undo_stack_;
  ProjectEdits edits_{project_, undo_stack_};
  QString path_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_PROJECT_DOCUMENT_H_
