/*
 * File:        project_edits.h
 * Module:      undo
 * Purpose:     Runs project editing operations as undoable commands,
 *              pushing only those that succeed
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_UNDO_PROJECT_EDITS_H_
#define SPEAK5220_GUI_UNDO_PROJECT_EDITS_H_

#include <QString>
#include <QUndoStack>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/project/phrase.h"
#include "core/project/project_editor.h"
#include "core/util/status.h"
#include "gui/undo/project_commands.h"

namespace speak5220::app {

// The GUI's way to edit a project (plan §2.5): each method makes the
// command for one IProjectEditor operation, runs it, and pushes it onto
// the undo stack only if it succeeded. A failure leaves the project and
// the stack as they were, and is returned for the caller to report.
//
// GUI thread only. The editor and the stack must outlive this object.
class ProjectEdits {
 public:
  ProjectEdits(core::project::IProjectEditor& editor, QUndoStack& stack);

  core::Status InsertPhrases(std::vector<core::project::PlacedPhrase> phrases);
  core::Status RemovePhrases(std::span<const core::project::PhraseId> ids);
  core::Status MovePhrases(std::span<const core::project::PhraseId> ids,
                           int destination);
  core::Status SortPhrases();
  core::Status RenamePhrase(const core::project::PhraseId& id,
                            std::string name);
  core::Status SetDescription(const core::project::PhraseId& id,
                              std::string description);
  core::Status SetPlayStart(const core::project::PhraseId& id, int sample);
  core::Status SetEncoding(const core::project::PhraseId& id,
                           core::project::EncodingChange change);
  // Changes several phrases' encodings as one step named `text`; all or
  // none (SetEncodingsCommand).
  core::Status SetEncodings(SetEncodingsCommand::Changes changes,
                            const QString& text);
  core::Status SetDefaultEncoder(core::encoder::EncoderOptions options);

 private:
  core::Status Push(std::unique_ptr<ProjectCommand> command);

  core::project::IProjectEditor* editor_;
  QUndoStack* stack_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_UNDO_PROJECT_EDITS_H_
