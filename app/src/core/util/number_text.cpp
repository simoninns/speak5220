/*
 * File:        number_text.cpp
 * Module:      util
 * Purpose:     Locale-independent conversion between numbers and text, for
 *              project files and option values
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/number_text.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <charconv>
#include <cmath>
#include <cstdint>
#include <locale>
#include <memory>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <system_error>

namespace speak5220::core {
namespace {

// Enough for the shortest form of any double ("-2.2250738585072014e-308").
constexpr std::size_t kNumberBufferSize = 32;

bool IsNumberCharacter(char c) {
  return (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.' ||
         c == 'e' || c == 'E';
}

}  // namespace

std::string FormatNumber(double value) {
  assert(std::isfinite(value));
  std::array<char, kNumberBufferSize> buffer = {};
  // std::to_chars without a precision gives the shortest text that reads
  // back exactly, and never depends on the locale.
  const std::to_chars_result result =
      std::to_chars(buffer.data(), std::to_address(buffer.end()), value);
  assert(result.ec == std::errc());
  return {buffer.data(), result.ptr};
}

std::optional<double> ParseNumber(std::string_view text) {
  // Only the characters of a decimal number, so "nan", "inf", hex and
  // spaces are refused before the stream sees them.
  if (text.empty()) {
    return std::nullopt;
  }
  for (const char c : text) {
    if (!IsNumberCharacter(c)) {
      return std::nullopt;
    }
  }
  // A stream imbued with the classic locale reads '.' as the decimal point
  // whatever the global locale. std::from_chars would do the same, but not
  // every standard library on the supported platforms has it for double.
  std::istringstream stream{std::string(text)};
  stream.imbue(std::locale::classic());
  double value = 0.0;
  stream >> value;
  if (stream.fail() || !std::isfinite(value)) {
    return std::nullopt;
  }
  // The whole text must have been read.
  if (stream.peek() != std::char_traits<char>::eof()) {
    return std::nullopt;
  }
  return value;
}

std::optional<std::int64_t> ParseInteger(std::string_view text) {
  // An optional minus sign, then digits only.
  const std::string_view digits = text.starts_with('-') ? text.substr(1) : text;
  if (digits.empty() || !std::ranges::all_of(digits, [](char c) {
        return c >= '0' && c <= '9';
      })) {
    return std::nullopt;
  }
  std::istringstream stream{std::string(text)};
  stream.imbue(std::locale::classic());
  std::int64_t value = 0;
  stream >> value;
  // Out-of-range values fail the stream.
  if (stream.fail() || stream.peek() != std::char_traits<char>::eof()) {
    return std::nullopt;
  }
  return value;
}

}  // namespace speak5220::core
