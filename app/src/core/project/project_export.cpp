/*
 * File:        project_export.cpp
 * Module:      project
 * Purpose:     Runs a project's export presets: the Acorn PHROM images of
 *              its PHROMs, one per file or concatenated
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_export.h"

#include <spdlog/fmt/fmt.h>

#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/packaging/acorn_phrom.h"
#include "core/project/phrom_allocator.h"
#include "core/util/logging.h"

namespace speak5220::core::project {

std::vector<std::string> PhromHeaderStrings(const ProjectSettings& settings,
                                            int number) {
  for (const PhromHeader& phrom : settings.phroms) {
    if (phrom.number == number) {
      return phrom.header_strings;
    }
  }
  std::vector<std::string> strings = settings.header_strings;
  if (strings.size() > 1) {
    strings[1] += " " + std::to_string(number);
  }
  return strings;
}

StatusOr<std::vector<packaging::PhromImage>> BuildAcornPhroms(
    const Project& project) {
  const PhromAllocation allocation = project.Allocation();
  const std::vector<Phrase>& phrases = project.Phrases();
  for (std::size_t i = 0; i < phrases.size(); ++i) {
    switch (allocation.phrases[i].placement) {
      case Placement::kPlaced:
        break;
      case Placement::kNotEncoded:
        return FailedPreconditionError(
            fmt::format("{} has not been encoded yet, so it cannot be exported",
                        phrases[i].name));
      case Placement::kTooLarge:
        return ResourceExhaustedError(fmt::format(
            "{} needs {} bytes, more than an empty PHROM holds ({}): shorten "
            "or re-encode it",
            phrases[i].name, allocation.phrases[i].cost, kPhromWordCapacity));
    }
  }
  std::vector<packaging::PhromImage> images;
  for (int number = 1; number <= allocation.PhromsUsed(); ++number) {
    const PhromFill& fill =
        allocation.phroms[static_cast<std::size_t>(number - 1)];
    std::vector<packaging::Word> words;
    std::set<std::string> names;
    for (const int index : fill.phrases) {
      const Phrase& phrase = phrases[static_cast<std::size_t>(index)];
      // A placed phrase always has frames.
      if (!phrase.encoding.has_value()) {
        return InternalError(phrase.name + " was placed without frames");
      }
      words.push_back({.name = phrase.name, .frames = phrase.encoding->frames});
      names.insert(phrase.name);
    }
    packaging::AcornPhromOptions options;
    options.strings = PhromHeaderStrings(project.Settings(), number);
    // Plan §4.1: only the entries whose word is in this PHROM.
    for (const auto& [character, name] : project.Settings().ascii_map) {
      if (names.contains(name)) {
        options.ascii_map[static_cast<int>(character)] = name;
      }
    }
    StatusOr<packaging::PhromImage> image =
        packaging::AcornPhrom::Build(words, options);
    if (!image.IsOk()) {
      return Status(
          image.GetStatus().Code(),
          fmt::format("PHROM {}: {}", number, image.GetStatus().Message()));
    }
    images.push_back(*std::move(image));
  }
  return images;
}

StatusOr<std::vector<ExportedFile>> RunExport(const Project& project,
                                              const ExportPreset& preset,
                                              std::string_view base) {
  if (preset.format != kAcornPhromFormat) {
    return InvalidArgumentError(
        fmt::format(R"(export format "{}" is not supported yet (only "{}"))",
                    preset.format, kAcornPhromFormat));
  }
  if (!preset.scope.empty() && preset.scope != kScopeAll) {
    return InvalidArgumentError(
        fmt::format(R"(export scope "{}" is not supported yet (only "{}"))",
                    preset.scope, kScopeAll));
  }
  const std::string_view layout =
      preset.layout.empty() ? kLayoutSeparate : std::string_view(preset.layout);
  if (layout != kLayoutSeparate && layout != kLayoutConcatenated) {
    return InvalidArgumentError(
        fmt::format(R"(PHROM layout "{}" is not "{}" or "{}")", layout,
                    kLayoutSeparate, kLayoutConcatenated));
  }
  StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(project);
  if (!images.IsOk()) {
    return images.GetStatus();
  }
  std::vector<ExportedFile> files;
  if (layout == kLayoutConcatenated) {
    ExportedFile file{.name = fmt::format("{}.bin", base), .bytes = {}};
    for (const packaging::PhromImage& image : *images) {
      file.bytes.insert(file.bytes.end(), image.bytes.begin(),
                        image.bytes.end());
    }
    files.push_back(std::move(file));
  } else {
    for (std::size_t i = 0; i < images->size(); ++i) {
      files.push_back({.name = fmt::format("{}_{}.bin", base, i + 1),
                       .bytes = std::move((*images)[i].bytes)});
    }
  }
  Logger(kLogModuleExport)
      ->info("export \"{}\": {} PHROM(s) in {} file(s)", preset.name,
             images->size(), files.size());
  return files;
}

}  // namespace speak5220::core::project
