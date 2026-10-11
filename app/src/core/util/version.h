/*
 * File:        version.h
 * Module:      util
 * Purpose:     Reports the application version shared by both executables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_VERSION_H_
#define SPEAK5220_CORE_UTIL_VERSION_H_

#include <string_view>

namespace speak5220::core {

// The application name, as shown to users.
inline constexpr std::string_view kApplicationName = "Speak5220";

// Returns the version: the short (7-character) git commit hash of the build,
// with "-dirty" appended when tracked files had uncommitted changes, or
// "unknown" for a build outside a git checkout.
// Thread-safe: returns a view of static storage.
std::string_view Version();

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_VERSION_H_
