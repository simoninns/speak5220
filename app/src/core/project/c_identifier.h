/*
 * File:        c_identifier.h
 * Module:      project
 * Purpose:     C identifiers derived from phrase names, for the header
 *              exports
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_C_IDENTIFIER_H_
#define SPEAK5220_CORE_PROJECT_C_IDENTIFIER_H_

#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace speak5220::core::project {

// Plan §4: the C identifier for a phrase name. Characters other than
// letters and digits become "_", runs of "_" collapse to one, leading and
// trailing "_" are dropped, and a name starting with a digit gets the
// prefix "W_" ("ONE,WON" -> "ONE_WON", "-ING" -> "ING", "2- (TWEN-)" ->
// "W_2_TWEN"). A name with no letter or digit gives "W".
std::string CIdentifierFromName(std::string_view name);

// The identifiers for a list of names, in order: CIdentifierFromName(),
// with a clash given the suffix "_2", "_3", ... (the first name keeps the
// plain form) until it is unique.
std::vector<std::string> CIdentifiersForNames(
    std::span<const std::string> names);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_C_IDENTIFIER_H_
