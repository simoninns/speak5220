/*
 * File:        file_digest.h
 * Module:      util
 * Purpose:     The SHA-256 of a file: the interface and its implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_FILE_DIGEST_H_
#define SPEAK5220_CORE_UTIL_FILE_DIGEST_H_

#include <filesystem>
#include <string>

#include "core/util/status.h"

namespace speak5220::core {

// Digests files. Implementations are thread-safe.
class IFileDigest {
 public:
  virtual ~IFileDigest() = default;

  // The SHA-256 of the file's bytes, as 64 lower-case hex digits. Fails if
  // the file cannot be read.
  virtual StatusOr<std::string> Sha256(const std::filesystem::path& path) = 0;
};

// IFileDigest reading the file in blocks. Stateless; thread-safe.
class FileSha256Digest : public IFileDigest {
 public:
  StatusOr<std::string> Sha256(const std::filesystem::path& path) override;
};

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_FILE_DIGEST_H_
