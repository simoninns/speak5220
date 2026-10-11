/*
 * File:        c_identifier.cpp
 * Module:      project
 * Purpose:     C identifiers derived from phrase names, for the header
 *              exports
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/c_identifier.h"

#include <set>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace speak5220::core::project {
namespace {

constexpr std::string_view kDigitPrefix = "W_";
constexpr std::string_view kEmptyIdentifier = "W";

bool IsLetterOrDigit(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
         (c >= '0' && c <= '9');
}

bool IsDigit(char c) { return c >= '0' && c <= '9'; }

}  // namespace

std::string CIdentifierFromName(std::string_view name) {
  std::string identifier;
  bool pending_underscore = false;
  for (const char c : name) {
    if (IsLetterOrDigit(c)) {
      // A run of other characters becomes one "_", and only between
      // letters or digits, so none leads or trails.
      if (pending_underscore && !identifier.empty()) {
        identifier += '_';
      }
      identifier += c;
      pending_underscore = false;
    } else {
      pending_underscore = true;
    }
  }
  if (identifier.empty()) {
    return std::string(kEmptyIdentifier);
  }
  if (IsDigit(identifier.front())) {
    identifier.insert(0, kDigitPrefix);
  }
  return identifier;
}

std::vector<std::string> CIdentifiersForNames(
    std::span<const std::string> names) {
  std::vector<std::string> identifiers;
  identifiers.reserve(names.size());
  std::set<std::string> used;
  for (const std::string& name : names) {
    const std::string base = CIdentifierFromName(name);
    std::string identifier = base;
    for (int n = 2; used.contains(identifier); ++n) {
      identifier = base + "_" + std::to_string(n);
    }
    used.insert(identifier);
    identifiers.push_back(identifier);
  }
  return identifiers;
}

}  // namespace speak5220::core::project
