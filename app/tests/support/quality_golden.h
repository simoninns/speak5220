/*
 * File:        quality_golden.h
 * Module:      test support
 * Purpose:     Reads the quality reference data in app/golden/quality/:
 *              PESQ, alignment and cheap measures per encoding, and the
 *              anchor scores
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_QUALITY_GOLDEN_H_
#define SPEAK5220_TESTS_SUPPORT_QUALITY_GOLDEN_H_

#include <filesystem>
#include <map>
#include <string>

#include "core/util/status.h"

namespace speak5220::testing {

// The quality reference data, app/golden/quality/.
std::filesystem::path QualityGoldenDir();

// One reference encoding's measures (app/scripts/golden/quality.py).
struct ReferenceQuality {
  std::string name;
  // The lag that lines the emulator's output up with the input.
  int lag = 0;
  // The output's length, first non-idle code to last.
  int samples = 0;
  // PESQ (MOS-LQO) as speechqual.py measures it.
  double pesq = 0.0;
  // evaluate.py's lvMeanAbs, lvBias, lvP90 and specDist.
  double level_err = 0.0;
  double level_bias = 0.0;
  double level_p90 = 0.0;
  double spec_dist = 0.0;
};

// The rows of scores/SET.tsv, by corpus file name.
core::StatusOr<std::map<std::string, ReferenceQuality>> ReadQualityScores(
    const std::filesystem::path& path);

// The anchor scores of anchor.tsv, by corpus file name.
core::StatusOr<std::map<std::string, double>> ReadAnchorScores(
    const std::filesystem::path& path);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_QUALITY_GOLDEN_H_
