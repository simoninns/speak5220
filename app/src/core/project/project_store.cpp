/*
 * File:        project_store.cpp
 * Module:      project
 * Purpose:     Where a project's files are kept: the interface, the path
 *              rules, and the choice between a ZIP file and a directory
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_store.h"

#include <cstddef>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>

#include "core/project/directory_project_store.h"
#include "core/project/zip_project_store.h"

namespace speak5220::core::project {
namespace {

// Longer paths are refused: no file of a project comes close.
constexpr std::size_t kMaxPathLength = 255;

}  // namespace

bool IsSafeProjectPath(std::string_view path) {
  if (path.empty() || path.size() > kMaxPathLength) {
    return false;
  }
  constexpr char kFirstPrintable = 0x20;
  constexpr char kLastPrintable = 0x7E;
  for (const char c : path) {
    if (c < kFirstPrintable || c > kLastPrintable || c == '\\' || c == ':') {
      return false;
    }
  }
  std::size_t start = 0;
  while (true) {
    const std::size_t end = path.find('/', start);
    const std::string_view component = path.substr(
        start, end == std::string_view::npos ? path.npos : end - start);
    if (component.empty() || component == "." || component == "..") {
      return false;
    }
    if (end == std::string_view::npos) {
      return true;
    }
    start = end + 1;
  }
}

std::unique_ptr<IProjectStore> MakeProjectStore(
    const std::filesystem::path& path) {
  if (path.extension().string() == kProjectExtension) {
    return std::make_unique<ZipProjectStore>(path);
  }
  return std::make_unique<DirectoryProjectStore>(path);
}

}  // namespace speak5220::core::project
