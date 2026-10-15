/*
 * File:        tms5220emu_command.h
 * Module:      test support
 * Purpose:     The tms5220emu command line on the application's emulator,
 *              for the reference-data and research scripts
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_TMS5220EMU_COMMAND_H_
#define SPEAK5220_TESTS_SUPPORT_TMS5220EMU_COMMAND_H_

#include <span>

namespace speak5220::testing {

// Runs the command line of the retired C emulator's tms5220emu program
// (docs/technical/emulator/command-line.md) on the application's emulator:
// scenario scripts and the bulk mode, with the same sample lines and event
// log. `arguments` is main()'s argv, the program name first. Returns the
// exit code: 0, or 2 for a command line or run that cannot be used.
// Re-implements tms5220emu.c main(); the C emulator is in git history at
// commit 6abe9ed.
int RunTms5220EmuCommand(std::span<char* const> arguments);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_TMS5220EMU_COMMAND_H_
