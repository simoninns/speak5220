/*
 * File:        phrom_import.cpp
 * Module:      project
 * Purpose:     Turns the words of Acorn PHROM images into locked phrases
 *              made from frames alone
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrom_import.h"

#include <spdlog/fmt/fmt.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/packaging/acorn_phrom.h"
#include "core/packaging/phrom_image.h"
#include "core/project/phrase_name.h"
#include "core/util/logging.h"

namespace speak5220::core::project {
namespace {

// A valid name for a read word that is not in `taken`: the image's name
// as it is if it is valid and free, otherwise one made from it.
std::string NameFor(const packaging::ReadWord& word,
                    const std::set<std::string>& taken) {
  if (word.name.empty()) {
    return SuggestPhraseName(fmt::format("WORD {}", word.number), taken);
  }
  StatusOr<std::string> normalised = NormalisePhraseName(word.name);
  if (normalised.IsOk() && !taken.contains(*normalised)) {
    return *std::move(normalised);
  }
  return SuggestPhraseName(normalised.IsOk() ? *normalised : word.name, taken);
}

}  // namespace

StatusOr<PhromImport> ImportPhromImage(std::span<const std::uint8_t> bytes,
                                       const std::set<std::string>& taken) {
  if (bytes.empty()) {
    return InvalidArgumentError("the PHROM image is empty");
  }
  if (bytes.size() % packaging::kPhromSize != 0) {
    return InvalidArgumentError(fmt::format(
        "a PHROM image is a multiple of 16384 bytes (16 KB per PHROM), not "
        "{}",
        bytes.size()));
  }
  const std::size_t banks = bytes.size() / packaging::kPhromSize;
  std::set<std::string> names = taken;
  PhromImport result;
  std::vector<Phrase>& phrases = result.phrases;
  for (std::size_t bank = 0; bank < banks; ++bank) {
    StatusOr<packaging::ReadPhrom> words = packaging::AcornPhrom::ReadImage(
        bytes.subspan(bank * packaging::kPhromSize, packaging::kPhromSize));
    if (!words.IsOk()) {
      if (banks == 1) {
        return words.GetStatus();
      }
      return Status(words.GetStatus().Code(),
                    fmt::format("PHROM {} of the image: {}", bank + 1,
                                words.GetStatus().Message()));
    }
    std::map<int, std::string> named;  // word number -> phrase name
    for (packaging::ReadWord& word : words->words) {
      Phrase phrase;
      phrase.id = NewPhraseId();
      phrase.name = NameFor(word, names);
      phrase.locked = true;
      phrase.encoding = CachedEncoding{.frames = std::move(word.frames)};
      names.insert(phrase.name);
      named.emplace(word.number, phrase.name);
      phrases.push_back(std::move(phrase));
    }
    for (const auto& [code, number] : words->ascii_words) {
      result.ascii_map.emplace(static_cast<char>(code), named.at(number));
    }
    result.header_strings.push_back(std::move(words->strings));
  }
  Logger(kLogModuleProject)
      ->info("read {} word(s) from {} PHROM image(s)", phrases.size(), banks);
  return result;
}

PhromSettings MergeImportedPhromSettings(const ProjectSettings& current,
                                         bool was_empty,
                                         const PhromImport& imported) {
  PhromSettings merged{.ascii_map = current.ascii_map,
                       .phroms = current.phroms};
  for (const auto& [character, name] : imported.ascii_map) {
    merged.ascii_map.emplace(character, name);
  }
  if (was_empty && current.phroms.empty() &&
      current.header_strings == ProjectSettings{}.header_strings) {
    for (std::size_t i = 0; i < imported.header_strings.size(); ++i) {
      if (!imported.header_strings[i].empty()) {
        merged.phroms.push_back({.number = static_cast<int>(i) + 1,
                                 .header_strings = imported.header_strings[i],
                                 .unknown = {}});
      }
    }
  }
  return merged;
}

}  // namespace speak5220::core::project
