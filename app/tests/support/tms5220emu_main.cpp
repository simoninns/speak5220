/*
 * File:        tms5220emu_main.cpp
 * Module:      test support
 * Purpose:     Entry point of tms5220emu, the emulator's command line for
 *              the reference-data and research scripts
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

// Built with the tests, never installed.

#include <cstddef>
#include <span>

#include "support/tms5220emu_command.h"

int main(int argc, char* argv[]) {
  return speak5220::testing::RunTms5220EmuCommand(
      std::span<char* const>(argv, static_cast<std::size_t>(argc)));
}
