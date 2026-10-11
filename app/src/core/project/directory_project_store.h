/*
 * File:        directory_project_store.h
 * Module:      project
 * Purpose:     Keeps a project unpacked, as a directory with the layout of
 *              its ZIP form
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_DIRECTORY_PROJECT_STORE_H_
#define SPEAK5220_CORE_PROJECT_DIRECTORY_PROJECT_STORE_H_

#include <filesystem>

#include "core/project/project_store.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// IProjectStore over a directory (plan §4): the form of the test fixtures,
// which can be diffed and reviewed. Saving writes a complete new directory
// beside the project (<path>.saving), then renames the old one to <path>~
// and the new one into place. A directory that holds files but no
// project.yaml is never replaced. Symbolic links inside the project are
// refused. Thread-compatible.
class DirectoryProjectStore : public IProjectStore {
 public:
  explicit DirectoryProjectStore(std::filesystem::path path);

  StatusOr<ProjectFiles> Read() override;
  Status Write(const ProjectFiles& files) override;

 private:
  std::filesystem::path path_;
};

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_DIRECTORY_PROJECT_STORE_H_
