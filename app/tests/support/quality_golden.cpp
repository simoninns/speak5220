/*
 * File:        quality_golden.cpp
 * Module:      test support
 * Purpose:     Reads the quality reference data in app/golden/quality/:
 *              PESQ, alignment and cheap measures per encoding, and the
 *              anchor scores
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/quality_golden.h"

#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/util/status.h"
#include "support/golden_data.h"

namespace speak5220::testing {

std::filesystem::path QualityGoldenDir() {
  return std::filesystem::path(SPEAK5220_GOLDEN_DIR) / "quality";
}

core::StatusOr<std::map<std::string, ReferenceQuality>> ReadQualityScores(
    const std::filesystem::path& path) {
  core::StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(path);
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  std::map<std::string, ReferenceQuality> out;
  for (const auto& row : *rows) {
    ReferenceQuality q;
    q.name = row.at("name");
    q.lag = std::stoi(row.at("lag"));
    q.samples = std::stoi(row.at("samples"));
    q.pesq = std::stod(row.at("pesq"));
    q.level_err = std::stod(row.at("level_err"));
    q.level_bias = std::stod(row.at("level_bias"));
    q.level_p90 = std::stod(row.at("level_p90"));
    q.spec_dist = std::stod(row.at("spec_dist"));
    out[q.name] = std::move(q);
  }
  return out;
}

core::StatusOr<std::map<std::string, double>> ReadAnchorScores(
    const std::filesystem::path& path) {
  core::StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(path);
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  std::map<std::string, double> out;
  for (const auto& row : *rows) {
    out[row.at("name")] = std::stod(row.at("pesq"));
  }
  return out;
}

}  // namespace speak5220::testing
