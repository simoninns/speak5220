/*
 * File:        golden_data.h
 * Module:      test support
 * Purpose:     Reads the reference data in app/golden/: codes, event logs,
 *              tables, images and frame lists
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_GOLDEN_DATA_H_
#define SPEAK5220_TESTS_SUPPORT_GOLDEN_DATA_H_

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "core/lpc/frame.h"
#include "core/util/status.h"

namespace speak5220::testing {

// The text lines of a file, without their line ends. A missing file is an
// error naming the path (TESTING.md: never a silent skip).
core::StatusOr<std::vector<std::string>> ReadLines(
    const std::filesystem::path& path);

// The bytes of a file.
core::StatusOr<std::vector<std::uint8_t>> ReadBytes(
    const std::filesystem::path& path);

// A file of integers, one per line (the golden .codes files).
core::StatusOr<std::vector<int>> ReadCodes(const std::filesystem::path& path);

// A tab-separated table with a header row: one map per row, keyed by the
// header's column names.
core::StatusOr<std::vector<std::map<std::string, std::string>>> ReadTable(
    const std::filesystem::path& path);

// A frame list in the CSV form of tms5220enc.py framesCsv():
// frame,kind,energy,repeat,pitch,k1..k10, blank where absent.
core::StatusOr<std::vector<core::lpc::Frame>> ReadFramesCsv(
    const std::filesystem::path& path);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_GOLDEN_DATA_H_
