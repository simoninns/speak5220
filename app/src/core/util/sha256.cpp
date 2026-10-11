/*
 * File:        sha256.cpp
 * Module:      util
 * Purpose:     SHA-256 digests, written as lower-case hex
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/sha256.h"

#include <QByteArray>
#include <QByteArrayView>
#include <QCryptographicHash>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace speak5220::core {

Sha256::Sha256()
    : hash_(std::make_unique<QCryptographicHash>(QCryptographicHash::Sha256)) {}

Sha256::~Sha256() = default;

void Sha256::Add(std::span<const std::uint8_t> bytes) {
  hash_->addData(
      QByteArrayView(bytes.data(), static_cast<qsizetype>(bytes.size())));
}

void Sha256::Add(std::string_view text) {
  hash_->addData(
      QByteArrayView(text.data(), static_cast<qsizetype>(text.size())));
}

std::string Sha256::Hex() const {
  return hash_->result().toHex().toStdString();
}

std::string Sha256Hex(std::span<const std::uint8_t> bytes) {
  Sha256 hash;
  hash.Add(bytes);
  return hash.Hex();
}

}  // namespace speak5220::core
