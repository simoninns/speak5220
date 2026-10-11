/*
 * File:        sha256.h
 * Module:      util
 * Purpose:     SHA-256 digests, written as lower-case hex
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_SHA256_H_
#define SPEAK5220_CORE_UTIL_SHA256_H_

#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

class QCryptographicHash;

namespace speak5220::core {

// An incremental SHA-256, over QtCore's QCryptographicHash. Not
// thread-safe: use one object per thread.
class Sha256 {
 public:
  Sha256();
  ~Sha256();
  Sha256(const Sha256&) = delete;
  Sha256& operator=(const Sha256&) = delete;

  void Add(std::span<const std::uint8_t> bytes);
  void Add(std::string_view text);

  // The digest of everything added, as 64 lower-case hex digits.
  std::string Hex() const;

 private:
  std::unique_ptr<QCryptographicHash> hash_;
};

// The SHA-256 of `bytes`, as 64 lower-case hex digits. Thread-safe.
std::string Sha256Hex(std::span<const std::uint8_t> bytes);

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_SHA256_H_
