/*
 * File:        directory_project_store.cpp
 * Module:      project
 * Purpose:     Keeps a project unpacked, as a directory with the layout of
 *              its ZIP form
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/directory_project_store.h"

#include <spdlog/fmt/fmt.h>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/util/logging.h"

namespace speak5220::core::project {
namespace {

namespace fs = std::filesystem;

constexpr std::string_view kSavingSuffix = ".saving";
constexpr std::string_view kProjectYaml = "project.yaml";

fs::path WithSuffix(const fs::path& path, std::string_view suffix) {
  fs::path result = path;
  result += suffix;
  return result;
}

StatusOr<std::vector<std::uint8_t>> ReadFile(const fs::path& path,
                                             std::uintmax_t size) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return UnavailableError("cannot open " + path.string());
  }
  // ifstream reads chars, so read into a string first.
  std::string buffer(static_cast<std::size_t>(size), '\0');
  file.read(buffer.data(), static_cast<std::streamsize>(size));
  if (std::cmp_not_equal(file.gcount(), size)) {
    return DataLossError("cannot read " + path.string());
  }
  return std::vector<std::uint8_t>(buffer.begin(), buffer.end());
}

Status WriteFile(const fs::path& path, const std::vector<std::uint8_t>& data) {
  std::error_code error;
  fs::create_directories(path.parent_path(), error);
  if (error) {
    return UnavailableError(fmt::format(
        "cannot create {}: {}", path.parent_path().string(), error.message()));
  }
  std::ofstream file(path, std::ios::binary | std::ios::trunc);
  const std::string buffer(data.begin(), data.end());
  file.write(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  file.close();
  if (!file) {
    return UnavailableError("cannot write " + path.string());
  }
  return OkStatus();
}

}  // namespace

DirectoryProjectStore::DirectoryProjectStore(std::filesystem::path path)
    : path_(std::move(path)) {}

StatusOr<ProjectFiles> DirectoryProjectStore::Read() {
  const std::string where = path_.string();
  std::error_code error;
  if (!fs::is_directory(path_, error)) {
    return NotFoundError(where + " is not a project directory");
  }
  ProjectFiles files;
  std::uint64_t total = 0;
  fs::recursive_directory_iterator it(path_, error);
  if (error) {
    return UnavailableError(
        fmt::format("cannot read {}: {}", where, error.message()));
  }
  for (; it != fs::recursive_directory_iterator(); it.increment(error)) {
    if (error) {
      return UnavailableError(
          fmt::format("cannot read {}: {}", where, error.message()));
    }
    const fs::directory_entry& entry = *it;
    const std::string relative =
        fs::relative(entry.path(), path_, error).generic_string();
    if (entry.is_symlink(error)) {
      return InvalidArgumentError(
          fmt::format("{} holds a symbolic link, \"{}\": refusing to open it",
                      where, relative));
    }
    if (entry.is_directory(error)) {
      continue;
    }
    if (!entry.is_regular_file(error) || !IsSafeProjectPath(relative)) {
      return InvalidArgumentError(
          fmt::format("{} holds \"{}\", which cannot be part of a project",
                      where, relative));
    }
    if (files.size() >= kMaxProjectFiles) {
      return OutOfRangeError(where + " holds too many files");
    }
    const std::uintmax_t size = entry.file_size(error);
    total += size;
    if (error || total > kMaxProjectBytes) {
      return OutOfRangeError(where + " is too large to open");
    }
    StatusOr<std::vector<std::uint8_t>> data = ReadFile(entry.path(), size);
    if (!data.IsOk()) {
      return data.GetStatus();
    }
    files.emplace(relative, *std::move(data));
  }
  Logger(kLogModuleProject)
      ->debug("read {} files ({} bytes) from {}", files.size(), total, where);
  return files;
}

Status DirectoryProjectStore::Write(const ProjectFiles& files) {
  const std::string where = path_.string();
  if (files.empty()) {
    return InvalidArgumentError("a project has at least one file");
  }
  for (const auto& [name, data] : files) {
    if (!IsSafeProjectPath(name)) {
      return InvalidArgumentError(
          fmt::format("refusing to write an unsafe path, \"{}\"", name));
    }
  }
  std::error_code error;
  const bool exists = fs::exists(path_, error);
  if (exists) {
    if (!fs::is_directory(path_, error)) {
      return FailedPreconditionError(where + " is not a directory");
    }
    if (!fs::is_empty(path_, error) &&
        !fs::exists(path_ / kProjectYaml, error)) {
      return FailedPreconditionError(fmt::format(
          "{} is not a Speak5220 project (it has no {}): not replacing it",
          where, kProjectYaml));
    }
  }
  // Write the whole new version first.
  const fs::path saving = WithSuffix(path_, kSavingSuffix);
  fs::remove_all(saving, error);
  if (error) {
    return UnavailableError(
        fmt::format("cannot clear {}: {}", saving.string(), error.message()));
  }
  for (const auto& [name, data] : files) {
    if (Status status = WriteFile(saving / fs::path(name), data);
        !status.IsOk()) {
      fs::remove_all(saving, error);
      return status;
    }
  }
  // Then swap it in, keeping the old version as the backup.
  const fs::path backup = WithSuffix(path_, kBackupSuffix);
  if (exists) {
    fs::remove_all(backup, error);
    fs::rename(path_, backup, error);
    if (error) {
      fs::remove_all(saving, error);
      return UnavailableError(fmt::format("cannot keep {} as {}: {}", where,
                                          backup.string(), error.message()));
    }
  }
  fs::rename(saving, path_, error);
  if (error) {
    const std::string message = error.message();
    if (exists) {
      fs::rename(backup, path_, error);
    }
    return UnavailableError(fmt::format("cannot save {}: {}", where, message));
  }
  Logger(kLogModuleProject)->debug("wrote {} files to {}", files.size(), where);
  return OkStatus();
}

}  // namespace speak5220::core::project
