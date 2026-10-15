/*
 * File:        phrase_name.h
 * Module:      project
 * Purpose:     The phrase name rules: Acorn's character set, the length
 *              limit, and names suggested for imported files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PHRASE_NAME_H_
#define SPEAK5220_CORE_PROJECT_PHRASE_NAME_H_

#include <set>
#include <string>
#include <string_view>

#include "core/util/status.h"

namespace speak5220::core::project {

// A name has at most 16 characters (the Acorn PHROM
// A's longest is 14).
inline constexpr int kMaxPhraseNameLength = 16;

// The characters of the Acorn PHROM A word list,
// upper-case A-Z, digits, space and ' ( ) , - . (a name is stored in a
// PHROM one byte per character, speak5220 datasheet §12.8).
bool IsPhraseNameCharacter(char c);

// The name as the project stores it: lower case converted to upper case,
// and leading and trailing spaces removed. Fails, with a message for the
// user, for an empty name, a name longer than kMaxPhraseNameLength, or any
// other character. Uniqueness is the project's to check.
StatusOr<std::string> NormalisePhraseName(std::string_view name);

// A valid name made from `stem` (an imported file's name without its
// extension) that is not in `taken`: upper case, each run of other
// characters (such as "_") turned into one space, cut to the length limit,
// "PHRASE" if nothing is left, and " 2", " 3", ... appended (cutting the
// name to make room) until it is unused.
std::string SuggestPhraseName(std::string_view stem,
                              const std::set<std::string>& taken);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PHRASE_NAME_H_
