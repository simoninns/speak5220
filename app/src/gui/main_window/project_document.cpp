/*
 * File:        project_document.cpp
 * Module:      main_window
 * Purpose:     The open project, its undo stack and its file
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/project_document.h"

#include <QFileInfo>
#include <QObject>
#include <QString>
#include <QUndoStack>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>

#include "core/audio/sndfile_wav_io.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_serializer.h"
#include "core/project/project_store.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::app {

namespace project = core::project;

namespace {

std::filesystem::path ToPath(const QString& text) {
  return {text.toStdU16String()};
}

}  // namespace

ProjectDocument::ProjectDocument(QObject* parent) : QObject(parent) {
  connect(&undo_stack_, &QUndoStack::indexChanged, this,
          &ProjectDocument::Changed);
  connect(&undo_stack_, &QUndoStack::cleanChanged, this,
          [this](bool clean) { emit ModifiedChanged(!clean); });
}

ProjectDocument::~ProjectDocument() {
  disconnect(&undo_stack_, nullptr, this, nullptr);
}

QString ProjectDocument::DisplayName() const {
  if (path_.isEmpty()) {
    return QString::fromLatin1(kUntitledName);
  }
  return QFileInfo(path_).fileName();
}

const project::Phrase* ProjectDocument::SavedPhrase(
    const project::PhraseId& id) const {
  return saved_.FindPhrase(id);
}

bool ProjectDocument::IsPhraseModified(const project::PhraseId& id) const {
  const project::Phrase* phrase = project_.FindPhrase(id);
  const project::Phrase* saved = saved_.FindPhrase(id);
  return phrase != nullptr && (saved == nullptr || *phrase != *saved);
}

void ProjectDocument::New() { Replace(project::Project(), QString()); }

core::Status ProjectDocument::Open(const QString& path) {
  QString project_path = QFileInfo(path).absoluteFilePath();
  if (QFileInfo(project_path).fileName() ==
      QString::fromLatin1(project::kProjectYamlPath)) {
    project_path = QFileInfo(project_path).absolutePath();
  }
  core::audio::SndfileWavCodec codec;
  project::ProjectSerializer serializer(codec);
  const std::unique_ptr<project::IProjectStore> store =
      project::MakeProjectStore(ToPath(project_path));
  core::StatusOr<project::Project> loaded = serializer.Load(*store);
  if (!loaded.IsOk()) {
    return {loaded.GetStatus().Code(), "cannot open " +
                                           project_path.toStdString() + ": " +
                                           loaded.GetStatus().Message()};
  }
  core::Logger(core::kLogModuleGui)
      ->info("opened {} ({} phrases)", project_path.toStdString(),
             loaded->Phrases().size());
  Replace(std::move(*loaded), project_path);
  return core::OkStatus();
}

core::Status ProjectDocument::Save() {
  if (path_.isEmpty()) {
    return core::FailedPreconditionError(
        "the project has not been saved yet: choose where to save it");
  }
  if (core::Status status = SaveTo(project_, path_); !status.IsOk()) {
    return status;
  }
  saved_ = project_;
  undo_stack_.setClean();
  emit SavedChanged();
  return core::OkStatus();
}

core::Status ProjectDocument::SaveAs(const QString& path) {
  QString file_path = QFileInfo(path).absoluteFilePath();
  const QString extension = QString::fromLatin1(project::kProjectExtension);
  if (!file_path.endsWith(extension, Qt::CaseInsensitive)) {
    file_path += extension;
  }
  project::Project saved = project_;
  if (saved.Settings().name.empty()) {
    project::ProjectSettings settings = saved.Settings();
    settings.name = QFileInfo(file_path).completeBaseName().toStdString();
    if (core::Status status = saved.SetSettings(std::move(settings));
        !status.IsOk()) {
      return status;
    }
  }
  if (core::Status status = SaveTo(saved, file_path); !status.IsOk()) {
    return status;
  }
  // The name is set outside the undo stack; the project is saved with it.
  project_ = std::move(saved);
  saved_ = project_;
  path_ = file_path;
  undo_stack_.setClean();
  emit PathChanged();
  emit SavedChanged();
  return core::OkStatus();
}

core::Status ProjectDocument::SaveTo(const project::Project& project,
                                     const QString& path) {
  core::audio::SndfileWavCodec codec;
  project::ProjectSerializer serializer(codec);
  const std::unique_ptr<project::IProjectStore> store =
      project::MakeProjectStore(ToPath(path));
  if (core::Status status = serializer.Save(project, *store); !status.IsOk()) {
    return {status.Code(),
            "cannot save " + path.toStdString() + ": " + status.Message()};
  }
  core::Logger(core::kLogModuleGui)->info("saved {}", path.toStdString());
  return core::OkStatus();
}

void ProjectDocument::Replace(project::Project project, const QString& path) {
  undo_stack_.clear();
  project_ = std::move(project);
  saved_ = project_;
  path_ = path;
  emit PathChanged();
  emit ModifiedChanged(false);
  emit Changed();
}

}  // namespace speak5220::app
