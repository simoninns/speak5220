/*
 * File:        project_store.h
 * Module:      project
 * Purpose:     Where a project's files are kept: the interface, the path
 *              rules, and the choice between a ZIP file and a directory
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PROJECT_STORE_H_
#define SPEAK5220_CORE_PROJECT_PROJECT_STORE_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::project {

// A project's files by path within the project ("project.yaml",
// "phrases/<id>/source.wav"), with "/" between components.
using ProjectFiles = std::map<std::string, std::vector<std::uint8_t>>;

// The extension of a project in its ZIP form.
inline constexpr std::string_view kProjectExtension = ".s5proj";

// The suffix of the copy of the previous version kept on each save.
inline constexpr std::string_view kBackupSuffix = "~";

// Limits on what a store reads, so a malicious archive cannot exhaust
// memory: far beyond a 64-PHROM project with its audio.
inline constexpr std::size_t kMaxProjectFiles = 100000;
inline constexpr std::uint64_t kMaxProjectBytes = std::uint64_t{1} << 30U;

// Reads and writes a whole project's files. Saving is atomic: a failure or
// a crash part-way leaves the previous version intact.
// Implementations are thread-compatible.
class IProjectStore {
 public:
  virtual ~IProjectStore() = default;

  // Every file of the project. Fails if the project cannot be read, holds
  // an unsafe path (IsSafeProjectPath()), or exceeds the limits above.
  virtual StatusOr<ProjectFiles> Read() = 0;

  // Replaces the project's files with `files`, keeping the previous
  // version beside it with kBackupSuffix. Fails for an unsafe path.
  virtual Status Write(const ProjectFiles& files) = 0;
};

// AGENTS.md §2: true for a relative path that stays inside the project.
// Components are separated by "/" and are not empty, "." or ".."; there
// is no leading "/", no "\", no ":" (drive letters, alternate streams),
// and only printable ASCII.
bool IsSafeProjectPath(std::string_view path);

// The store for `path`: a ZIP file when `path` ends in kProjectExtension,
// the directory form otherwise.
std::unique_ptr<IProjectStore> MakeProjectStore(
    const std::filesystem::path& path);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PROJECT_STORE_H_
