/*
 * File:        logging_options.h
 * Module:      util
 * Purpose:     The --log-level, --log-file and --no-console-log options shared
 *              by both executables
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_LOGGING_OPTIONS_H_
#define SPEAK5220_CORE_UTIL_LOGGING_OPTIONS_H_

#include "core/util/logging.h"
#include "core/util/status.h"

class QCommandLineParser;

namespace speak5220::core {

// Adds the logging options of plan §2.6 to `parser`.
void AddLoggingOptions(QCommandLineParser& parser);

// Reads the logging options from a parser that has processed its
// arguments. An option not given takes its value from `defaults`: the
// GUI's preferences (plan §2.6), so a command-line option overrides the
// preference for that run. Fails for an unknown level, or for
// --no-console-log with no log file.
StatusOr<LogSettings> LogSettingsFromParser(const QCommandLineParser& parser,
                                            const LogSettings& defaults = {});

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_LOGGING_OPTIONS_H_
