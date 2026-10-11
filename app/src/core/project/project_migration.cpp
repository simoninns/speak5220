/*
 * File:        project_migration.cpp
 * Module:      project
 * Purpose:     The project format's name and version, and the migration of
 *              older project.yaml documents to the current version
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_migration.h"

#include <spdlog/fmt/fmt.h>
#include <yaml-cpp/yaml.h>

#include <cstddef>
#include <span>
#include <string>

#include "core/util/logging.h"

namespace speak5220::core::project {

std::span<const MigrationStep> ProjectMigrationSteps() { return {}; }

Status MigrateProjectDocument(YAML::Node& root, int version,
                              std::span<const MigrationStep> steps) {
  const int latest = 1 + static_cast<int>(steps.size());
  if (version < 1) {
    return InvalidArgumentError(
        fmt::format("project format version {} does not exist", version));
  }
  if (version > latest) {
    return FailedPreconditionError(fmt::format(
        "the project was saved by a newer version of Speak5220 (format "
        "version {}; this version reads up to {})",
        version, latest));
  }
  for (int from = version; from < latest; ++from) {
    const MigrationStep& step = steps[static_cast<std::size_t>(from - 1)];
    if (Status status = step(root); !status.IsOk()) {
      return {status.Code(),
              fmt::format("cannot upgrade the project from format version "
                          "{}: {}",
                          from, status.Message())};
    }
    Logger(kLogModuleProject)
        ->info("upgraded the project from format version {} to {}", from,
               from + 1);
  }
  root["version"] = std::to_string(latest);
  return OkStatus();
}

}  // namespace speak5220::core::project
