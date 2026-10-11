/*
 * File:        project_commands.h
 * Module:      undo
 * Purpose:     One undo command per project editing operation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_UNDO_PROJECT_COMMANDS_H_
#define SPEAK5220_GUI_UNDO_PROJECT_COMMANDS_H_

#include <QUndoCommand>
#include <string>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/project/phrase.h"
#include "core/project/project_editor.h"
#include "core/util/status.h"

namespace speak5220::app {

// An undo command over one IProjectEditor operation (plan §2.5). Execute()
// runs the operation the first time, before the command is pushed, so a
// failed operation is never pushed (ProjectEdits). The redo() that
// QUndoStack::push() then calls does nothing; later redo() and undo()
// calls apply the operation and its reverse.
//
// Undoing or redoing from the state the command left cannot fail, since
// each operation returns its exact reverse. If it does anyway, the error
// is logged and the command is made obsolete, so the stack drops it.
//
// GUI thread only. The editor must outlive the command.
class ProjectCommand : public QUndoCommand {
 public:
  ProjectCommand(const ProjectCommand&) = delete;
  ProjectCommand& operator=(const ProjectCommand&) = delete;
  ~ProjectCommand() override = default;

  // Runs the operation for the first time. On failure nothing has changed.
  core::Status Execute();

  void redo() final;
  void undo() final;

 protected:
  ProjectCommand(core::project::IProjectEditor& editor, const QString& text);

  core::project::IProjectEditor& Editor() { return *editor_; }

  // The operation, and its reverse.
  virtual core::Status Apply() = 0;
  virtual core::Status Revert() = 0;

 private:
  void Report(const core::Status& status, const char* step);

  core::project::IProjectEditor* editor_;
  // Set by a successful Execute(): the next redo() is push()'s.
  bool skip_next_redo_ = false;
};

// InsertPhrases(); reversed by RemovePhrases() with their ids.
class InsertPhrasesCommand : public ProjectCommand {
 public:
  InsertPhrasesCommand(core::project::IProjectEditor& editor,
                       std::vector<core::project::PlacedPhrase> phrases);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  std::vector<core::project::PlacedPhrase> phrases_;
};

// RemovePhrases(); reversed by InsertPhrases() with what it returned.
class RemovePhrasesCommand : public ProjectCommand {
 public:
  RemovePhrasesCommand(core::project::IProjectEditor& editor,
                       std::vector<core::project::PhraseId> ids);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  std::vector<core::project::PhraseId> ids_;
  std::vector<core::project::PlacedPhrase> removed_;
};

// MovePhrases(); reversed by SetOrder() with the previous order.
class MovePhrasesCommand : public ProjectCommand {
 public:
  MovePhrasesCommand(core::project::IProjectEditor& editor,
                     std::vector<core::project::PhraseId> ids, int destination);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  std::vector<core::project::PhraseId> ids_;
  int destination_;
  std::vector<core::project::PhraseId> previous_order_;
};

// SortPhrases(); reversed by SetOrder() with the previous order.
class SortPhrasesCommand : public ProjectCommand {
 public:
  explicit SortPhrasesCommand(core::project::IProjectEditor& editor);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  std::vector<core::project::PhraseId> previous_order_;
};

// RenamePhrase(); reversed by RenamePhrase() with the previous name.
class RenamePhraseCommand : public ProjectCommand {
 public:
  RenamePhraseCommand(core::project::IProjectEditor& editor,
                      core::project::PhraseId id, std::string name);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  core::project::PhraseId id_;
  std::string name_;
  std::string previous_name_;
};

// SetDescription(); reversed by SetDescription() with the previous text.
class SetDescriptionCommand : public ProjectCommand {
 public:
  SetDescriptionCommand(core::project::IProjectEditor& editor,
                        core::project::PhraseId id, std::string description);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  core::project::PhraseId id_;
  std::string description_;
  std::string previous_description_;
};

// SetPlayStart(); reversed by SetPlayStart() with the previous sample.
class SetPlayStartCommand : public ProjectCommand {
 public:
  SetPlayStartCommand(core::project::IProjectEditor& editor,
                      core::project::PhraseId id, int sample);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  core::project::PhraseId id_;
  int sample_;
  int previous_sample_ = 0;
};

// SetEncoding(); reversed by SetEncoding() with the previous settings and
// encoding.
class SetEncodingCommand : public ProjectCommand {
 public:
  SetEncodingCommand(core::project::IProjectEditor& editor,
                     core::project::PhraseId id,
                     core::project::EncodingChange change);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  core::project::PhraseId id_;
  core::project::EncodingChange change_;
  core::project::EncodingChange previous_;
};

// One encoding change per phrase, as one step (Optimise, re-encoding a
// selection): SetEncoding() on each in order; if one fails, those
// already changed are put back and the command fails. Reversed by
// SetEncoding() with each previous settings and encoding, in reverse
// order.
class SetEncodingsCommand : public ProjectCommand {
 public:
  using Changes = std::vector<
      std::pair<core::project::PhraseId, core::project::EncodingChange>>;

  SetEncodingsCommand(core::project::IProjectEditor& editor, Changes changes,
                      const QString& text);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  Changes changes_;
  std::vector<core::project::EncodingChange> previous_;
};

// SetDefaultEncoder(); reversed by SetDefaultEncoder() with the previous
// options.
class SetDefaultEncoderCommand : public ProjectCommand {
 public:
  SetDefaultEncoderCommand(core::project::IProjectEditor& editor,
                           core::encoder::EncoderOptions options);

 protected:
  core::Status Apply() override;
  core::Status Revert() override;

 private:
  core::encoder::EncoderOptions options_;
  core::encoder::EncoderOptions previous_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_UNDO_PROJECT_COMMANDS_H_
