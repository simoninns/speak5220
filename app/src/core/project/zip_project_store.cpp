/*
 * File:        zip_project_store.cpp
 * Module:      project
 * Purpose:     Keeps a project as one .s5proj ZIP file, over libzip
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/zip_project_store.h"

#include <spdlog/fmt/fmt.h>
#include <zip.h>

#include <cstdint>
#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/util/logging.h"

namespace speak5220::core::project {
namespace {

// A fixed entry time, 1980-01-01 00:00 (the earliest MS-DOS date), so
// saving the same project gives the same archive.
constexpr zip_uint16_t kDosTime = 0x0000;
constexpr zip_uint16_t kDosDate = 0x0021;

struct ArchiveDiscarder {
  void operator()(zip_t* archive) const { zip_discard(archive); }
};
using ArchiveHandle = std::unique_ptr<zip_t, ArchiveDiscarder>;

struct EntryCloser {
  void operator()(zip_file_t* file) const { zip_fclose(file); }
};
using EntryHandle = std::unique_ptr<zip_file_t, EntryCloser>;

std::string OpenErrorText(int code) {
  zip_error_t error;
  zip_error_init_with_code(&error, code);
  std::string text = zip_error_strerror(&error);
  zip_error_fini(&error);
  return text;
}

// libzip's cancel callback: asks the store's check.
int CancelCallback(zip_t* /*archive*/, void* user_data) {
  const auto* check = static_cast<const std::function<bool()>*>(user_data);
  return (*check)() ? 1 : 0;
}

}  // namespace

ZipProjectStore::ZipProjectStore(std::filesystem::path path)
    : path_(std::move(path)) {}

void ZipProjectStore::SetCancelCheck(std::function<bool()> check) {
  cancel_check_ = std::move(check);
}

StatusOr<ProjectFiles> ZipProjectStore::Read() {
  const std::string where = path_.string();
  int error_code = 0;
  ArchiveHandle archive(
      zip_open(where.c_str(), ZIP_RDONLY | ZIP_CHECKCONS, &error_code));
  if (!archive) {
    if (error_code == ZIP_ER_NOENT || error_code == ZIP_ER_OPEN) {
      return NotFoundError(
          fmt::format("cannot open {}: {}", where, OpenErrorText(error_code)));
    }
    return DataLossError(fmt::format("{} is not a usable project archive: {}",
                                     where, OpenErrorText(error_code)));
  }
  const zip_int64_t count = zip_get_num_entries(archive.get(), 0);
  if (count < 0 || std::cmp_greater(count, kMaxProjectFiles)) {
    return OutOfRangeError(where + " holds too many files");
  }
  ProjectFiles files;
  std::uint64_t total = 0;
  for (zip_int64_t i = 0; i < count; ++i) {
    const auto index = static_cast<zip_uint64_t>(i);
    zip_stat_t stat;
    zip_stat_init(&stat);
    if (zip_stat_index(archive.get(), index, 0, &stat) != 0 ||
        (stat.valid & ZIP_STAT_NAME) == 0 ||
        (stat.valid & ZIP_STAT_SIZE) == 0) {
      return DataLossError(fmt::format("{}: cannot read entry {}", where, i));
    }
    std::string name = stat.name;
    // Directory entries carry no data; their names must be safe too.
    const bool directory = !name.empty() && name.back() == '/';
    if (directory) {
      name.pop_back();
    }
    if (!IsSafeProjectPath(name)) {
      return InvalidArgumentError(
          fmt::format("{} holds an unsafe path, \"{}\": refusing to open it",
                      where, stat.name));
    }
    if (directory) {
      continue;
    }
    total += stat.size;
    if (stat.size > kMaxProjectBytes || total > kMaxProjectBytes) {
      return OutOfRangeError(where + " is too large to open");
    }
    std::vector<std::uint8_t> data(static_cast<std::size_t>(stat.size));
    EntryHandle entry(zip_fopen_index(archive.get(), index, 0));
    if (!entry) {
      return DataLossError(fmt::format("{}: cannot open \"{}\": {}", where,
                                       name, zip_strerror(archive.get())));
    }
    const zip_int64_t got = zip_fread(entry.get(), data.data(), stat.size);
    if (std::cmp_not_equal(got, stat.size)) {
      return DataLossError(fmt::format("{}: \"{}\" is damaged", where, name));
    }
    if (!files.emplace(name, std::move(data)).second) {
      return DataLossError(fmt::format("{} holds \"{}\" twice", where, name));
    }
  }
  Logger(kLogModuleProject)
      ->debug("read {} files ({} bytes) from {}", files.size(), total, where);
  return files;
}

Status ZipProjectStore::Write(const ProjectFiles& files) {
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
  if (std::filesystem::is_directory(path_, error)) {
    return FailedPreconditionError(where + " is a directory");
  }
  if (std::filesystem::exists(path_, error)) {
    std::filesystem::path backup = path_;
    backup += kBackupSuffix;
    std::filesystem::copy_file(
        path_, backup, std::filesystem::copy_options::overwrite_existing,
        error);
    if (error) {
      return UnavailableError(fmt::format("cannot keep a copy of {} as {}: {}",
                                          where, backup.string(),
                                          error.message()));
    }
  }
  int error_code = 0;
  ArchiveHandle archive(
      zip_open(where.c_str(), ZIP_CREATE | ZIP_TRUNCATE, &error_code));
  if (!archive) {
    return UnavailableError(
        fmt::format("cannot write {}: {}", where, OpenErrorText(error_code)));
  }
  for (const auto& [name, data] : files) {
    zip_source_t* source =
        zip_source_buffer(archive.get(), data.data(), data.size(), 0);
    if (source == nullptr) {
      return InternalError(fmt::format("cannot add \"{}\": {}", name,
                                       zip_strerror(archive.get())));
    }
    const zip_int64_t index =
        zip_file_add(archive.get(), name.c_str(), source, ZIP_FL_ENC_UTF_8);
    if (index < 0) {
      zip_source_free(source);
      return InternalError(fmt::format("cannot add \"{}\": {}", name,
                                       zip_strerror(archive.get())));
    }
    zip_file_set_dostime(archive.get(), static_cast<zip_uint64_t>(index),
                         kDosTime, kDosDate, 0);
  }
  if (cancel_check_) {
    zip_register_cancel_callback_with_state(archive.get(), CancelCallback,
                                            nullptr, &cancel_check_);
  }
  // zip_close() writes a temporary file and renames it over the old one
  // only when it is complete. On failure the archive is discarded, which
  // removes the temporary file and leaves the old one as it was.
  zip_t* const closing = archive.release();
  if (zip_close(closing) != 0) {
    const std::string message = zip_strerror(closing);
    zip_discard(closing);
    return UnavailableError(fmt::format("cannot save {}: {}", where, message));
  }
  Logger(kLogModuleProject)->debug("wrote {} files to {}", files.size(), where);
  return OkStatus();
}

}  // namespace speak5220::core::project
