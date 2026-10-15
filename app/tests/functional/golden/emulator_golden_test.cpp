/*
 * File:        emulator_golden_test.cpp
 * Module:      emulator tests
 * Purpose:     Runs the C++ emulator on every reference run in app/golden/
 *              and compares codes and events with the C reference's
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "gtest/gtest.h"
#include "support/emulator_run.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

const std::filesystem::path kEmulatorGolden =
    std::filesystem::path(SPEAK5220_GOLDEN_DIR) / "emulator";

// The first index at which two sequences differ, or their common length.
template <typename T>
std::size_t FirstDifference(const std::vector<T>& a, const std::vector<T>& b) {
  std::size_t i = 0;
  while (i < a.size() && i < b.size() && a[i] == b[i]) {
    ++i;
  }
  return i;
}

// The C++ emulator gives identical codes sample
// for sample, and identical events slot for slot, on every run of the
// reference emulator: its self-test scenarios, the synthetic vectors and
// encoder output, through both paths.
TEST(EmulatorGoldenTest, MatchesReferenceSampleForSampleAndEventForEvent) {
  const core::StatusOr<std::vector<std::map<std::string, std::string>>> runs =
      ReadTable(kEmulatorGolden / "runs.tsv");
  ASSERT_TRUE(runs.IsOk()) << runs.GetStatus().ToString();
  ASSERT_FALSE(runs->empty());
  for (const auto& row : *runs) {
    const std::string& name = row.at("name");
    SCOPED_TRACE(name);
    const std::string& mode = row.at("mode");
    const std::filesystem::path input = kEmulatorGolden / row.at("input");
    const core::StatusOr<EmulatorRun> run =
        mode == "script"
            ? RunScenarioScript(input)
            : RunBulk(input, mode == "external", std::stoi(row.at("phase")));
    ASSERT_TRUE(run.IsOk()) << run.GetStatus().ToString();
    const core::StatusOr<std::vector<int>> codes =
        ReadCodes(kEmulatorGolden / "runs" / (name + ".codes"));
    const core::StatusOr<std::vector<std::string>> events =
        ReadLines(kEmulatorGolden / "runs" / (name + ".events"));
    ASSERT_TRUE(codes.IsOk()) << codes.GetStatus().ToString();
    ASSERT_TRUE(events.IsOk()) << events.GetStatus().ToString();

    const std::size_t sample = FirstDifference(run->codes, *codes);
    EXPECT_EQ(run->codes.size(), codes->size());
    EXPECT_EQ(sample, std::min(run->codes.size(), codes->size()))
        << "first different sample " << sample;
    const std::size_t event = FirstDifference(run->events, *events);
    EXPECT_EQ(run->events.size(), events->size());
    if (event < run->events.size() && event < events->size()) {
      ADD_FAILURE() << "first different event " << event << ": got \""
                    << run->events[event] << "\", expected \""
                    << (*events)[event] << "\"";
    }
  }
}

}  // namespace
}  // namespace speak5220::testing
