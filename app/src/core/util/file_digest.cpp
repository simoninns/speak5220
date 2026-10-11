/*
 * File:        file_digest.cpp
 * Module:      util
 * Purpose:     The SHA-256 of a file: the interface and its implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/file_digest.h"

#include <array>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>

#include "core/util/sha256.h"

namespace speak5220::core {
namespace {

constexpr std::size_t kBlockSize = 65536;

}  // namespace

StatusOr<std::string> FileSha256Digest::Sha256(
    const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return NotFoundError("cannot open " + path.string());
  }
  core::Sha256 hash;
  std::array<char, kBlockSize> block = {};
  while (file) {
    file.read(block.data(), block.size());
    const auto got = static_cast<std::size_t>(file.gcount());
    hash.Add(std::string_view(block.data(), got));
  }
  if (file.bad()) {
    return DataLossError("cannot read " + path.string());
  }
  return hash.Hex();
}

}  // namespace speak5220::core
