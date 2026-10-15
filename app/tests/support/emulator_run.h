/*
 * File:        emulator_run.h
 * Module:      test support
 * Purpose:     Runs the emulator as the retired tms5220emu program did,
 *              producing the same codes and event log
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_EMULATOR_RUN_H_
#define SPEAK5220_TESTS_SUPPORT_EMULATOR_RUN_H_

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "core/emulator/chip_driver.h"
#include "core/emulator/tms5220_event.h"
#include "core/util/status.h"

namespace speak5220::testing {

// The output of one run: the code of every sample period from power-up,
// and the event log, one line per event in tms5220emu's format
// (docs/technical/emulator/command-line.md, "Output formats"). The retired
// C emulator's tms5220emu.c is in git history at commit 6abe9ed.
struct EmulatorRun {
  std::vector<int> codes;
  std::vector<std::string> events;
};

// An event as a tms5220emu event-log line, such as "13 VSM LOAD 0".
std::string FormatEvent(const core::emulator::Tms5220Event& event);

// Runs a scenario script (tms5220emu SCRIPT). `load` paths are relative to
// the script's directory.
core::StatusOr<EmulatorRun> RunScenarioScript(
    const std::filesystem::path& script);

// A bulk-mode run (tms5220emu --image IMAGE --addr ADDRESS ...).
struct BulkRun {
  std::filesystem::path image;
  // Where the image is loaded (--load-at), and the phrase spoken (--addr).
  std::uint32_t load_at = 0;
  std::uint32_t address = 0;
  // A scenario script run after loading, before the phrase (--prior).
  std::optional<std::filesystem::path> prior;
  // The frame clock's position at power-up (--phase).
  int phase = 0;
  // Speak External instead of Speak (--external).
  bool external = false;
  // --speak-at, --margin and --reset.
  core::emulator::SpeakOptions speak;
};

// Speaks the phrase at `run.address` from power-up, as tms5220emu's bulk
// mode does.
core::StatusOr<EmulatorRun> RunBulk(const BulkRun& run);

// Speaks the phrase in `image`, loaded at address 0, from power-up at
// frame-clock `phase` (--image IMAGE --addr 0 --phase PHASE [--external]).
core::StatusOr<EmulatorRun> RunBulk(const std::filesystem::path& image,
                                    bool external, int phase);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_EMULATOR_RUN_H_
