/*
 * File:        cli_commands.h
 * Module:      cli
 * Purpose:     The speak5220-cli commands: new, add, import-phrom, encode,
 *              export, synth and measure
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CLI_CLI_COMMANDS_H_
#define SPEAK5220_CLI_CLI_COMMANDS_H_

#include <QStringList>

class QCommandLineParser;

namespace speak5220::cli {

// Exit codes (docs/application/reference/cli-options.md).
inline constexpr int kExitSuccess = 0;
inline constexpr int kExitFailure = 1;
inline constexpr int kExitUsage = 2;

// Adds the options the commands take to `parser`.
void AddCommandOptions(QCommandLineParser& parser);

// Runs the command named by the first positional argument, with the rest
// as its arguments. Messages go to stdout, errors to stderr. Returns the
// exit code.
int RunCommand(const QCommandLineParser& parser, const QStringList& arguments);

}  // namespace speak5220::cli

#endif  // SPEAK5220_CLI_CLI_COMMANDS_H_
