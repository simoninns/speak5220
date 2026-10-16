/*
 * File:        project_edits.cpp
 * Module:      undo
 * Purpose:     Runs project editing operations as undoable commands,
 *              pushing only those that succeed
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/undo/project_edits.h"

#include <QString>
#include <QUndoStack>
#include <memory>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/project/phrase.h"
#include "core/project/project_editor.h"
#include "core/util/status.h"
#include "gui/undo/project_commands.h"

namespace speak5220::app {

namespace project = core::project;

ProjectEdits::ProjectEdits(project::IProjectEditor& editor, QUndoStack& stack)
    : editor_(&editor), stack_(&stack) {}

core::Status ProjectEdits::InsertPhrases(
    std::vector<project::PlacedPhrase> phrases) {
  return Push(
      std::make_unique<InsertPhrasesCommand>(*editor_, std::move(phrases)));
}

core::Status ProjectEdits::RemovePhrases(
    std::span<const project::PhraseId> ids) {
  return Push(std::make_unique<RemovePhrasesCommand>(
      *editor_, std::vector<project::PhraseId>(ids.begin(), ids.end())));
}

core::Status ProjectEdits::MovePhrases(std::span<const project::PhraseId> ids,
                                       int destination) {
  return Push(std::make_unique<MovePhrasesCommand>(
      *editor_, std::vector<project::PhraseId>(ids.begin(), ids.end()),
      destination));
}

core::Status ProjectEdits::SortPhrases() {
  return Push(std::make_unique<SortPhrasesCommand>(*editor_));
}

core::Status ProjectEdits::RenamePhrase(const project::PhraseId& id,
                                        std::string name) {
  return Push(
      std::make_unique<RenamePhraseCommand>(*editor_, id, std::move(name)));
}

core::Status ProjectEdits::SetDescription(const project::PhraseId& id,
                                          std::string description) {
  return Push(std::make_unique<SetDescriptionCommand>(*editor_, id,
                                                      std::move(description)));
}

core::Status ProjectEdits::SetPlayStart(const project::PhraseId& id,
                                        int sample) {
  return Push(std::make_unique<SetPlayStartCommand>(*editor_, id, sample));
}

core::Status ProjectEdits::SetEncoding(const project::PhraseId& id,
                                       project::EncodingChange change) {
  return Push(
      std::make_unique<SetEncodingCommand>(*editor_, id, std::move(change)));
}

core::Status ProjectEdits::SetEncodings(SetEncodingsCommand::Changes changes,
                                        const QString& text) {
  return Push(std::make_unique<SetEncodingsCommand>(*editor_,
                                                    std::move(changes), text));
}

core::Status ProjectEdits::SetDefaultEncoder(
    core::encoder::EncoderOptions options) {
  return Push(std::make_unique<SetDefaultEncoderCommand>(*editor_, options));
}

core::Status ProjectEdits::ImportPhrom(
    std::vector<project::PlacedPhrase> phrases,
    project::PhromSettings settings) {
  return Push(std::make_unique<ImportPhromCommand>(*editor_, std::move(phrases),
                                                   std::move(settings)));
}

core::Status ProjectEdits::ReplacePhrases(std::vector<project::Phrase> phrases,
                                          const QString& text) {
  return Push(std::make_unique<ReplacePhrasesCommand>(
      *editor_, std::move(phrases), text));
}

core::Status ProjectEdits::SetProjectSettings(project::ProjectIdentity identity,
                                              project::PhromSettings settings) {
  return Push(std::make_unique<SetProjectSettingsCommand>(
      *editor_, std::move(identity), std::move(settings)));
}

core::Status ProjectEdits::SetExports(
    std::vector<project::ExportPreset> exports, const QString& text) {
  return Push(
      std::make_unique<SetExportsCommand>(*editor_, std::move(exports), text));
}

core::Status ProjectEdits::Push(std::unique_ptr<ProjectCommand> command) {
  if (core::Status status = command->Execute(); !status.IsOk()) {
    return status;
  }
  // The stack takes ownership; its redo() call is skipped (ProjectCommand).
  stack_->push(command.release());
  return core::OkStatus();
}

}  // namespace speak5220::app
