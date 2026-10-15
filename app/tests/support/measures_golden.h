/*
 * File:        measures_golden.h
 * Module:      test support
 * Purpose:     Readers for the quality measures' reference data, and the
 *              subset's encodings prepared as the measures prepare them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_MEASURES_GOLDEN_H_
#define SPEAK5220_TESTS_SUPPORT_MEASURES_GOLDEN_H_

#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "core/quality/signal_preparation.h"
#include "core/util/status.h"

namespace speak5220::testing {

// The quality measures' reference data, app/golden/quality/measures/.
std::filesystem::path MeasuresGoldenDir();

// One reference encoding's measures (app/scripts/golden/measures.py).
struct ReferenceMeasures {
  std::string name;
  int lag = 0;
  int speech_frames = 0;
  double lsd_db = 0.0;
  double lsd_over_2db = 0.0;
  double lsd_over_4db = 0.0;
  double reaper_gpe = 0.0;
  double reaper_f0_rmse_cents = 0.0;
  double reaper_vde = 0.0;
  double harvest_gpe = 0.0;
  double harvest_f0_rmse_cents = 0.0;
  double harvest_vde = 0.0;
  double estoi = 0.0;
};

// The rows of scores/SET.tsv, by file name.
core::StatusOr<std::map<std::string, ReferenceMeasures>> ReadMeasureScores(
    const std::filesystem::path& path);

// The masks of masks/SET.tsv, by file name.
core::StatusOr<std::map<std::string, std::vector<bool>>> ReadMasks(
    const std::filesystem::path& path);

// One file's F0 tracks (tracks/SET/NAME.tsv), every 10 ms, 0 where
// unvoiced.
struct ReferenceTracks {
  std::vector<double> reaper_original;
  std::vector<double> reaper_output;
  std::vector<double> harvest_original;
  std::vector<double> harvest_output;
};
core::StatusOr<ReferenceTracks> ReadTracks(const std::filesystem::path& path);

// One subset file's reference encoding in one option set, spoken by the
// emulator and prepared for the measures with libfvad.
struct PreparedEncoding {
  std::string set_name;
  std::string name;
  // The alignment's lag, as measures.py gives it: the output, from its
  // first non-idle code, is delayed by this many samples.
  int lag = 0;
  core::quality::PreparedSignals prepared;
};

// Every subset file's reference encoding in every option set, prepared.
// Fails if the reference data cannot be read or an encoding cannot be
// spoken.
core::StatusOr<std::vector<PreparedEncoding>> PrepareReferenceEncodings();

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_MEASURES_GOLDEN_H_
