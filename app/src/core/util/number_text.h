/*
 * File:        number_text.h
 * Module:      util
 * Purpose:     Locale-independent conversion between numbers and text, for
 *              project files and option values
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_NUMBER_TEXT_H_
#define SPEAK5220_CORE_UTIL_NUMBER_TEXT_H_

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace speak5220::core {

// The shortest text that reads back as exactly `value`, with a '.' as the
// decimal point whatever the locale ("20", "0.35", "1e-09"). `value` must
// be finite. Thread-safe.
std::string FormatNumber(double value);

// Reads a decimal number written in the C locale ("20", "-0.5", "1e-09"),
// whatever the current locale. The whole text must be the number: leading
// or trailing characters, an empty text, "nan" and "inf" give nullopt.
// Thread-safe.
std::optional<double> ParseNumber(std::string_view text);

// Reads a whole number in decimal ("240", "-3"). As ParseNumber(), the
// whole text must be the number, and it must fit in 64 bits. Thread-safe.
std::optional<std::int64_t> ParseInteger(std::string_view text);

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_NUMBER_TEXT_H_
