/*
 * File:        dialogs.cpp
 * Module:      main_window
 * Purpose:     The questions and messages the main window puts to the
 *              user, behind an interface so tests can answer them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/dialogs.h"

#include <QDir>
#include <QFileDialog>
#include <QMessageBox>
#include <QString>
#include <QStringList>
#include <optional>

namespace speak5220::app {

namespace {

// The longest list of names a confirmation shows before "and N more".
constexpr qsizetype kMaxListedNames = 10;

QString NameList(const QStringList& names) {
  QStringList shown = names.mid(0, kMaxListedNames);
  if (names.size() > kMaxListedNames) {
    shown << QStringLiteral("and %1 more").arg(names.size() - kMaxListedNames);
  }
  return shown.join(QLatin1Char('\n'));
}

}  // namespace

std::optional<QString> QtDialogs::OpenProjectPath() {
  QString path = QFileDialog::getOpenFileName(
      parent_, QStringLiteral("Open Project"), QString(),
      QStringLiteral("Speak5220 projects (*.s5proj project.yaml);;"
                     "All files (*)"));
  if (path.isEmpty()) {
    return std::nullopt;
  }
  return path;
}

std::optional<QString> QtDialogs::SaveProjectPath(const QString& suggested) {
  QString path = QFileDialog::getSaveFileName(
      parent_, QStringLiteral("Save Project"), suggested,
      QStringLiteral("Speak5220 projects (*.s5proj)"));
  if (path.isEmpty()) {
    return std::nullopt;
  }
  return path;
}

QStringList QtDialogs::ImportFilePaths() {
  return QFileDialog::getOpenFileNames(
      parent_, QStringLiteral("Add Phrases"), QString(),
      QStringLiteral("WAV files (*.wav *.WAV);;All files (*)"));
}

SaveChoice QtDialogs::AskSaveChanges(const QString& name) {
  const QMessageBox::StandardButton button = QMessageBox::warning(
      parent_, QStringLiteral("Unsaved Changes"),
      QStringLiteral("Save the changes to %1?").arg(name),
      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel,
      QMessageBox::Save);
  switch (button) {
    case QMessageBox::Save:
      return SaveChoice::kSave;
    case QMessageBox::Discard:
      return SaveChoice::kDiscard;
    default:
      return SaveChoice::kCancel;
  }
}

bool QtDialogs::ConfirmRemove(const QStringList& names) {
  const QString question =
      names.size() == 1 ? QStringLiteral("Remove %1?").arg(names.front())
                        : QStringLiteral("Remove these %1 phrases?\n\n%2")
                              .arg(names.size())
                              .arg(NameList(names));
  return QMessageBox::question(parent_, QStringLiteral("Remove Phrases"),
                               question, QMessageBox::Yes | QMessageBox::Cancel,
                               QMessageBox::Cancel) == QMessageBox::Yes;
}

bool QtDialogs::AskNormalise(const QStringList& files) {
  return QMessageBox::question(
             parent_, QStringLiteral("Normalise Audio"),
             QStringLiteral("These files clip or are very quiet (peak below "
                            "-30 dBFS):\n\n%1\n\nNormalise them to a peak "
                            "of -1 dBFS? Otherwise they are added "
                            "unchanged.")
                 .arg(NameList(files)),
             QMessageBox::Yes | QMessageBox::No,
             QMessageBox::Yes) == QMessageBox::Yes;
}

std::optional<ExportRequest> QtDialogs::AskExport(const ExportStart& start) {
  // The question is parented to the dialog, which stays open if the user
  // declines.
  QWidget* question_parent = parent_;
  ExportStart asking = start;
  asking.confirm_overwrite = [&question_parent](const QString& directory,
                                                const QStringList& names) {
    return ConfirmOverwrite(question_parent, directory, names);
  };
  ExportDialog dialog(asking, parent_);
  question_parent = &dialog;
  if (dialog.exec() != QDialog::Accepted) {
    return std::nullopt;
  }
  return dialog.Request();
}

bool QtDialogs::ConfirmOverwrite(QWidget* parent, const QString& directory,
                                 const QStringList& names) {
  const QString question =
      names.size() == 1
          ? QStringLiteral("%1 already exists in %2. Replace it?")
                .arg(names.front(), QDir::toNativeSeparators(directory))
          : QStringLiteral(
                "These %1 files already exist in %2:\n\n%3\n\n"
                "Replace them?")
                .arg(names.size())
                .arg(QDir::toNativeSeparators(directory), NameList(names));
  return QMessageBox::question(parent, QStringLiteral("Replace Files"),
                               question, QMessageBox::Yes | QMessageBox::Cancel,
                               QMessageBox::Cancel) == QMessageBox::Yes;
}

void QtDialogs::ShowError(const QString& title, const QString& message) {
  QMessageBox::warning(parent_, title, message);
}

}  // namespace speak5220::app
