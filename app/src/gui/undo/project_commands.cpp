/*
 * File:        project_commands.cpp
 * Module:      undo
 * Purpose:     One undo command per project editing operation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/undo/project_commands.h"

#include <QString>
#include <QUndoCommand>
#include <string>
#include <utility>
#include <vector>

#include "core/project/phrase.h"
#include "core/project/project_editor.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

QString ToQString(const std::string& text) {
  return QString::fromStdString(text);
}

QString InsertText(const std::vector<project::PlacedPhrase>& phrases) {
  if (phrases.size() == 1) {
    return QStringLiteral("Add %1").arg(ToQString(phrases.front().phrase.name));
  }
  return QStringLiteral("Add %1 Phrases").arg(phrases.size());
}

QString CountText(const QString& verb, std::size_t count) {
  return count == 1 ? QStringLiteral("%1 Phrase").arg(verb)
                    : QStringLiteral("%1 %2 Phrases").arg(verb).arg(count);
}

std::vector<project::PhraseId> IdsOf(
    const std::vector<project::PlacedPhrase>& phrases) {
  std::vector<project::PhraseId> ids;
  ids.reserve(phrases.size());
  for (const project::PlacedPhrase& placed : phrases) {
    ids.push_back(placed.phrase.id);
  }
  return ids;
}

}  // namespace

ProjectCommand::ProjectCommand(project::IProjectEditor& editor,
                               const QString& text)
    : QUndoCommand(text), editor_(&editor) {}

core::Status ProjectCommand::Execute() {
  core::Status status = Apply();
  skip_next_redo_ = status.IsOk();
  return status;
}

void ProjectCommand::redo() {
  if (skip_next_redo_) {
    skip_next_redo_ = false;
    return;
  }
  Report(Apply(), "redo");
}

void ProjectCommand::undo() { Report(Revert(), "undo"); }

void ProjectCommand::Report(const core::Status& status, const char* step) {
  if (status.IsOk()) {
    return;
  }
  core::Logger(core::kLogModuleGui)
      ->error("cannot {} \"{}\": {}", step, text().toStdString(),
              status.Message());
  setObsolete(true);
}

InsertPhrasesCommand::InsertPhrasesCommand(
    project::IProjectEditor& editor, std::vector<project::PlacedPhrase> phrases)
    : ProjectCommand(editor, InsertText(phrases)),
      phrases_(std::move(phrases)) {}

core::Status InsertPhrasesCommand::Apply() {
  return Editor().InsertPhrases(phrases_);
}

core::Status InsertPhrasesCommand::Revert() {
  const std::vector<project::PhraseId> ids = IdsOf(phrases_);
  return Editor().RemovePhrases(ids).GetStatus();
}

RemovePhrasesCommand::RemovePhrasesCommand(project::IProjectEditor& editor,
                                           std::vector<project::PhraseId> ids)
    : ProjectCommand(editor, CountText(QStringLiteral("Remove"), ids.size())),
      ids_(std::move(ids)) {}

core::Status RemovePhrasesCommand::Apply() {
  core::StatusOr<std::vector<project::PlacedPhrase>> removed =
      Editor().RemovePhrases(ids_);
  if (!removed.IsOk()) {
    return removed.GetStatus();
  }
  removed_ = std::move(*removed);
  return core::OkStatus();
}

core::Status RemovePhrasesCommand::Revert() {
  return Editor().InsertPhrases(removed_);
}

MovePhrasesCommand::MovePhrasesCommand(project::IProjectEditor& editor,
                                       std::vector<project::PhraseId> ids,
                                       int destination)
    : ProjectCommand(editor, CountText(QStringLiteral("Move"), ids.size())),
      ids_(std::move(ids)),
      destination_(destination) {}

core::Status MovePhrasesCommand::Apply() {
  core::StatusOr<std::vector<project::PhraseId>> previous =
      Editor().MovePhrases(ids_, destination_);
  if (!previous.IsOk()) {
    return previous.GetStatus();
  }
  previous_order_ = std::move(*previous);
  return core::OkStatus();
}

core::Status MovePhrasesCommand::Revert() {
  return Editor().SetOrder(previous_order_).GetStatus();
}

SortPhrasesCommand::SortPhrasesCommand(project::IProjectEditor& editor)
    : ProjectCommand(editor, QStringLiteral("Sort Phrases A-Z")) {}

core::Status SortPhrasesCommand::Apply() {
  core::StatusOr<std::vector<project::PhraseId>> previous =
      Editor().SortPhrases();
  if (!previous.IsOk()) {
    return previous.GetStatus();
  }
  previous_order_ = std::move(*previous);
  return core::OkStatus();
}

core::Status SortPhrasesCommand::Revert() {
  return Editor().SetOrder(previous_order_).GetStatus();
}

RenamePhraseCommand::RenamePhraseCommand(project::IProjectEditor& editor,
                                         project::PhraseId id, std::string name)
    : ProjectCommand(editor,
                     QStringLiteral("Rename to %1").arg(ToQString(name))),
      id_(std::move(id)),
      name_(std::move(name)) {}

core::Status RenamePhraseCommand::Apply() {
  core::StatusOr<std::string> previous = Editor().RenamePhrase(id_, name_);
  if (!previous.IsOk()) {
    return previous.GetStatus();
  }
  previous_name_ = std::move(*previous);
  return core::OkStatus();
}

core::Status RenamePhraseCommand::Revert() {
  return Editor().RenamePhrase(id_, previous_name_).GetStatus();
}

SetDescriptionCommand::SetDescriptionCommand(project::IProjectEditor& editor,
                                             project::PhraseId id,
                                             std::string description)
    : ProjectCommand(editor, QStringLiteral("Change Description")),
      id_(std::move(id)),
      description_(std::move(description)) {}

core::Status SetDescriptionCommand::Apply() {
  core::StatusOr<std::string> previous =
      Editor().SetDescription(id_, description_);
  if (!previous.IsOk()) {
    return previous.GetStatus();
  }
  previous_description_ = std::move(*previous);
  return core::OkStatus();
}

core::Status SetDescriptionCommand::Revert() {
  return Editor().SetDescription(id_, previous_description_).GetStatus();
}

SetPlayStartCommand::SetPlayStartCommand(project::IProjectEditor& editor,
                                         project::PhraseId id, int sample)
    : ProjectCommand(editor, QStringLiteral("Move Play Start")),
      id_(std::move(id)),
      sample_(sample) {}

core::Status SetPlayStartCommand::Apply() {
  const core::StatusOr<int> previous = Editor().SetPlayStart(id_, sample_);
  if (!previous.IsOk()) {
    return previous.GetStatus();
  }
  previous_sample_ = *previous;
  return core::OkStatus();
}

core::Status SetPlayStartCommand::Revert() {
  return Editor().SetPlayStart(id_, previous_sample_).GetStatus();
}

SetEncodingCommand::SetEncodingCommand(project::IProjectEditor& editor,
                                       project::PhraseId id,
                                       project::EncodingChange change)
    : ProjectCommand(editor, QStringLiteral("Change Encoder Settings")),
      id_(std::move(id)),
      change_(std::move(change)) {}

core::Status SetEncodingCommand::Apply() {
  core::StatusOr<project::EncodingChange> previous =
      Editor().SetEncoding(id_, change_);
  if (!previous.IsOk()) {
    return previous.GetStatus();
  }
  previous_ = std::move(*previous);
  return core::OkStatus();
}

core::Status SetEncodingCommand::Revert() {
  return Editor().SetEncoding(id_, previous_).GetStatus();
}

}  // namespace speak5220::app
