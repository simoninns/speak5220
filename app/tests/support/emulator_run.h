/*
 * File:        emulator_run.h
 * Module:      test support
 * Purpose:     Runs the C++ emulator as the reference tms5220emu program
 *              does, producing the same codes and event log
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_EMULATOR_RUN_H_
#define SPEAK5220_TESTS_SUPPORT_EMULATOR_RUN_H_

#include <filesystem>
#include <string>
#include <vector>

#include "core/emulator/tms5220_event.h"
#include "core/util/status.h"

namespace speak5220::testing {

// The output of one run: the code of every sample period from power-up,
// and the event log, one line per event in tms5220emu's format
// (docs/technical/emulator/command-line.md, "Output formats").
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

// Speaks the phrase in `image`, loaded at address 0, from power-up at
// frame-clock `phase`, as tms5220emu's bulk mode does (--image IMAGE
// --addr 0 --phase PHASE [--external]).
core::StatusOr<EmulatorRun> RunBulk(const std::filesystem::path& image,
                                    bool external, int phase);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_EMULATOR_RUN_H_
