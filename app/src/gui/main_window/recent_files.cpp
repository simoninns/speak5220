/*
 * File:        recent_files.cpp
 * Module:      main_window
 * Purpose:     The File > Open Recent list and where it is kept
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/recent_files.h"

#include <QSettings>
#include <QString>
#include <QStringList>
#include <QVariant>

namespace speak5220::app {

QStringList SettingsRecentFilesStore::Load() {
  return settings_.value(QString::fromLatin1(kRecentFilesKey)).toStringList();
}

void SettingsRecentFilesStore::Save(const QStringList& paths) {
  settings_.setValue(QString::fromLatin1(kRecentFilesKey), paths);
}

RecentFiles::RecentFiles(const QStringList& paths) {
  for (const QString& path : paths) {
    if (!path.isEmpty() && !paths_.contains(path) &&
        paths_.size() < kMaxEntries) {
      paths_.append(path);
    }
  }
}

void RecentFiles::Add(const QString& path) {
  paths_.removeAll(path);
  paths_.prepend(path);
  while (paths_.size() > kMaxEntries) {
    paths_.removeLast();
  }
}

void RecentFiles::Remove(const QString& path) { paths_.removeAll(path); }

}  // namespace speak5220::app
