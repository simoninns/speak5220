/*
 * File:        status_text.h
 * Module:      main_window
 * Purpose:     The status bar's summary of a project's size in KB and in
 *              PHROMs
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_STATUS_TEXT_H_
#define SPEAK5220_GUI_MAIN_WINDOW_STATUS_TEXT_H_

#include <QString>

#include "core/project/phrom_allocator.h"

namespace speak5220::app {

// "37.2 KB · 3 PHROMs of 64 · 4.1 KB free in PHROM 3".
// The size is every PHROM in use, overheads included
// (PhromAllocation::TotalBytes()), in units of 1024 bytes with one decimal.
// "No PHROMs in use" when nothing is placed. Appended when they apply:
// "over budget", the number of phrases too large for a PHROM, and the
// number not yet encoded. Numbers are written in the C locale. Pure.
QString ProjectSizeText(const core::project::PhromAllocation& allocation);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_STATUS_TEXT_H_
