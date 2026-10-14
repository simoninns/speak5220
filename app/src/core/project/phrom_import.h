/*
 * File:        phrom_import.h
 * Module:      project
 * Purpose:     Turns the words of Acorn PHROM images into locked phrases
 *              made from frames alone
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PHROM_IMPORT_H_
#define SPEAK5220_CORE_PROJECT_PHROM_IMPORT_H_

#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// The words of one or more concatenated 16 KB Acorn PHROM images
// (packaging::AcornPhrom::Read(), speak5220 datasheet §12.8) as locked
// phrases with no source audio (plan §3.4), in image then word order. Each
// phrase gets a NewPhraseId(), its name from the image made valid
// (NormalisePhraseName(), or SuggestPhraseName() when that fails or the
// name is taken; "WORD <number>" for a word with no name) and unique
// against `taken` and the other phrases, and an encoding holding the read
// frames, ending in the stop, with empty encoder_version and
// settings_hash. Fails for empty input, a size that is not a multiple of
// 16384, or a bank that cannot be read (the message names the bank, from 1,
// when there are several). Pure apart from the ids; thread-safe.
// O(bytes + words log words).
// What ImportPhromImage() reads. A plain value type.
struct PhromImport {
  std::vector<Phrase> phrases;
  // The images' ASCII entries that point at a word, each naming the phrase
  // that word became (the first image's, when several map a character).
  std::map<char, std::string> ascii_map;
  // Each image's header strings, in image order.
  std::vector<std::vector<std::string>> header_strings;
};

StatusOr<PhromImport> ImportPhromImage(std::span<const std::uint8_t> bytes,
                                       const std::set<std::string>& taken);

// The PHROM settings of `current` with what `imported` brings: its ASCII
// entries for the characters `current` does not map; and, when the project
// had no phrases (`was_empty`) and still has the default header strings
// and no per-PHROM ones, each image's header strings as PHROM n's, so
// exporting the words again gives back the images. Pure.
PhromSettings MergeImportedPhromSettings(const ProjectSettings& current,
                                         bool was_empty,
                                         const PhromImport& imported);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PHROM_IMPORT_H_
