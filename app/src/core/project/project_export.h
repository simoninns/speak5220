/*
 * File:        project_export.h
 * Module:      project
 * Purpose:     Runs a project's export presets: the Acorn PHROM images of
 *              its PHROMs, one per file or concatenated
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PROJECT_EXPORT_H_
#define SPEAK5220_CORE_PROJECT_PROJECT_EXPORT_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/packaging/phrom_image.h"
#include "core/project/project.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// Export formats and their options (plan §6). Only the Acorn PHROM images
// are built so far; the other formats of plan §6 are not.
inline constexpr std::string_view kAcornPhromFormat = "acorn-phrom";
inline constexpr std::string_view kScopeAll = "all";
inline constexpr std::string_view kLayoutSeparate = "separate";
inline constexpr std::string_view kLayoutConcatenated = "concatenated";

// A file an export produces: its name (no directory) and its bytes.
struct ExportedFile {
  std::string name = {};
  std::vector<std::uint8_t> bytes = {};

  friend bool operator==(const ExportedFile& a,
                         const ExportedFile& b) = default;
};

// Plan §4.1: the header strings of PHROM `number` (from 1): its own, if
// the project sets them, or the project's with " <number>" appended to the
// second string.
std::vector<std::string> PhromHeaderStrings(const ProjectSettings& settings,
                                            int number);

// One Acorn PHROM image per PHROM the project fills (plan §4.1, speak5220
// datasheet §12.8), from the phrases' current frames: word numbers from
// 127 in list order, each PHROM with the ASCII entries whose phrase it
// holds. Fails if a phrase has no frames or is too large for a PHROM.
StatusOr<std::vector<packaging::PhromImage>> BuildAcornPhroms(
    const Project& project);

// Runs `preset` over every phrase. Files are named from `base`:
// "<base>_1.bin", "<base>_2.bin", ... for separate PHROM images, or
// "<base>.bin" for concatenated ones. Fails for a format, scope or layout
// that is not supported yet, and as BuildAcornPhroms().
StatusOr<std::vector<ExportedFile>> RunExport(const Project& project,
                                              const ExportPreset& preset,
                                              std::string_view base);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PROJECT_EXPORT_H_
