/*
 * File:        phrase_name.cpp
 * Module:      project
 * Purpose:     The phrase name rules: Acorn's character set, the length
 *              limit, and names suggested for imported files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrase_name.h"

#include <spdlog/fmt/fmt.h>

#include <cstddef>
#include <set>
#include <string>
#include <string_view>

namespace speak5220::core::project {
namespace {

constexpr std::string_view kPunctuation = " '(),-.";
constexpr std::string_view kFallbackName = "PHRASE";

bool IsLower(char c) { return c >= 'a' && c <= 'z'; }

char ToUpper(char c) {
  return IsLower(c) ? static_cast<char>(c - 'a' + 'A') : c;
}

std::string_view TrimSpaces(std::string_view text) {
  const std::size_t first = text.find_first_not_of(' ');
  if (first == std::string_view::npos) {
    return {};
  }
  const std::size_t last = text.find_last_not_of(' ');
  return text.substr(first, last - first + 1);
}

// The character as the user would recognise it in a message: printable
// ASCII as itself, anything else as a byte value.
std::string Describe(char c) {
  const auto byte = static_cast<unsigned char>(c);
  constexpr unsigned char kFirstPrintable = 0x20;
  constexpr unsigned char kLastPrintable = 0x7E;
  if (byte >= kFirstPrintable && byte <= kLastPrintable) {
    return fmt::format("\"{}\"", c);
  }
  return fmt::format("byte 0x{:02X}", byte);
}

}  // namespace

bool IsPhraseNameCharacter(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
         kPunctuation.find(c) != std::string_view::npos;
}

StatusOr<std::string> NormalisePhraseName(std::string_view name) {
  const std::string_view trimmed = TrimSpaces(name);
  if (trimmed.empty()) {
    return InvalidArgumentError("a phrase name cannot be empty");
  }
  std::string result;
  result.reserve(trimmed.size());
  for (const char c : trimmed) {
    const char upper = ToUpper(c);
    if (!IsPhraseNameCharacter(upper)) {
      return InvalidArgumentError(fmt::format(
          "a phrase name cannot contain {}: use A-Z, 0-9, space and ' ( ) , "
          "- .",
          Describe(c)));
    }
    result += upper;
  }
  if (result.size() > static_cast<std::size_t>(kMaxPhraseNameLength)) {
    return InvalidArgumentError(fmt::format(
        "\"{}\" is {} characters long; a phrase name has at most {}", result,
        result.size(), kMaxPhraseNameLength));
  }
  return result;
}

std::string SuggestPhraseName(std::string_view stem,
                              const std::set<std::string>& taken) {
  std::string base;
  bool gap = false;
  for (const char c : stem) {
    const char upper = ToUpper(c);
    if (IsPhraseNameCharacter(upper) && upper != ' ') {
      if (gap && !base.empty()) {
        base += ' ';
      }
      base += upper;
      gap = false;
    } else {
      gap = true;
    }
  }
  base = std::string(TrimSpaces(std::string_view(base).substr(
      0, static_cast<std::size_t>(kMaxPhraseNameLength))));
  if (base.empty()) {
    base = kFallbackName;
  }
  if (!taken.contains(base)) {
    return base;
  }
  for (int n = 2;; ++n) {
    const std::string suffix = " " + std::to_string(n);
    const std::size_t room =
        static_cast<std::size_t>(kMaxPhraseNameLength) - suffix.size();
    std::string candidate =
        std::string(TrimSpaces(std::string_view(base).substr(0, room))) +
        suffix;
    if (!taken.contains(candidate)) {
      return candidate;
    }
  }
}

}  // namespace speak5220::core::project
