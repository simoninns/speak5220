/*
 * File:        project_migration.h
 * Module:      project
 * Purpose:     The project format's name and version, and the migration of
 *              older project.yaml documents to the current version
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PROJECT_MIGRATION_H_
#define SPEAK5220_CORE_PROJECT_PROJECT_MIGRATION_H_

#include <yaml-cpp/yaml.h>

#include <functional>
#include <span>
#include <string_view>

#include "core/util/status.h"

namespace speak5220::core::project {

// project.yaml's "format" value, and the version this build writes.
inline constexpr std::string_view kProjectFormatName = "speak5220-project";
inline constexpr int kProjectFormatVersion = 1;

// Upgrades a parsed project.yaml from one version to the next, in place.
using MigrationStep = std::function<Status(YAML::Node& root)>;

// The steps this build knows: step i upgrades version i + 1 to i + 2, so
// there are kProjectFormatVersion - 1 of them. Version 1 is the first, so
// there are none yet.
std::span<const MigrationStep> ProjectMigrationSteps();

// Brings `root`, a project.yaml of format version `version`, up to version
// 1 + steps.size() by running the steps from `version` on, and sets its
// "version" to the result. Fails for a version below 1, a version newer
// than the steps reach (written by a newer Speak5220), or a failing step.
Status MigrateProjectDocument(YAML::Node& root, int version,
                              std::span<const MigrationStep> steps);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PROJECT_MIGRATION_H_
