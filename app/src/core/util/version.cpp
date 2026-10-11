/*
 * File:        version.cpp
 * Module:      util
 * Purpose:     Reports the application version shared by both executables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/version.h"

#include <string_view>

#include "core/util/version_string.h"

namespace speak5220::core {

std::string_view Version() { return kVersionString; }

}  // namespace speak5220::core
