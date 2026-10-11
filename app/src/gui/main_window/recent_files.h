/*
 * File:        recent_files.h
 * Module:      main_window
 * Purpose:     The File > Open Recent list and where it is kept
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_RECENT_FILES_H_
#define SPEAK5220_GUI_MAIN_WINDOW_RECENT_FILES_H_

#include <QSettings>
#include <QString>
#include <QStringList>

namespace speak5220::app {

// Keeps the recently opened projects between runs. Implementations are
// used on the GUI thread only.
class IRecentFilesStore {
 public:
  virtual ~IRecentFilesStore() = default;

  // The list as last saved, newest first; empty if none.
  virtual QStringList Load() = 0;
  virtual void Save(const QStringList& paths) = 0;
};

// IRecentFilesStore in the application's QSettings (the platform's usual
// place for preferences), under kRecentFilesKey.
class SettingsRecentFilesStore : public IRecentFilesStore {
 public:
  static constexpr const char* kRecentFilesKey = "recentFiles";

  QStringList Load() override;
  void Save(const QStringList& paths) override;

 private:
  QSettings settings_;
};

// The list itself: the newest first, without repeats, at most kMaxEntries.
// A plain value type.
class RecentFiles {
 public:
  static constexpr int kMaxEntries = 8;

  RecentFiles() = default;
  // A list read from a store, tidied: empty entries and repeats dropped,
  // and cut to kMaxEntries.
  explicit RecentFiles(const QStringList& paths);

  const QStringList& Paths() const { return paths_; }

  // Puts `path` first, removing an earlier entry for it, and drops the
  // oldest beyond kMaxEntries.
  void Add(const QString& path);
  void Remove(const QString& path);
  void Clear() { paths_.clear(); }

 private:
  QStringList paths_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_RECENT_FILES_H_
