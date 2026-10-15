/*
 * File:        zip_project_store.h
 * Module:      project
 * Purpose:     Keeps a project as one .s5proj ZIP file, over libzip
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_ZIP_PROJECT_STORE_H_
#define SPEAK5220_CORE_PROJECT_ZIP_PROJECT_STORE_H_

#include <filesystem>
#include <functional>

#include "core/project/project_store.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// IProjectStore over a ZIP file, which any ZIP tool can open.
// Saving writes a temporary file beside the project and renames it over
// the old one only when it is complete (libzip's commit), after copying
// the old one to <path>~. Entries are written in path order with a fixed
// timestamp, so the same project gives the same bytes. Thread-compatible.
class ZipProjectStore : public IProjectStore {
 public:
  explicit ZipProjectStore(std::filesystem::path path);

  StatusOr<ProjectFiles> Read() override;
  Status Write(const ProjectFiles& files) override;

  // Called repeatedly while an archive is being written; returning true
  // abandons the save, which then fails and leaves the previous file as it
  // was. Lets a long save be cancelled, and a crash part-way through be
  // simulated in tests.
  void SetCancelCheck(std::function<bool()> check);

 private:
  std::filesystem::path path_;
  std::function<bool()> cancel_check_;
};

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_ZIP_PROJECT_STORE_H_
