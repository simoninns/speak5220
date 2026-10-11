/*
 * File:        project_builder.h
 * Module:      test support
 * Purpose:     Builds phrases and projects for tests: fixed ids, frames
 *              that pack to a chosen size, phrases with or without audio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_PROJECT_BUILDER_H_
#define SPEAK5220_TESTS_SUPPORT_PROJECT_BUILDER_H_

#include <string>
#include <vector>

#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/project.h"

namespace speak5220::testing {

// A valid phrase id that differs for each n: "00000000-0000-0000-0000-
// <n as 12 hex digits>".
core::project::PhraseId TestId(int n);

// Frames that pack to exactly `bytes` bytes (at least 1): silence frames
// and a stop, four bits each.
std::vector<core::lpc::Frame> FramesPackingTo(int bytes);

// The packed bytes a phrase called `name` must have to cost `cost` bytes
// in a PHROM.
int BytesForCost(int cost, const std::string& name);

// A phrase with id TestId(n), a short audio clip and source information,
// not encoded.
core::project::Phrase AudioPhrase(int n, const std::string& name);

// A phrase like AudioPhrase(), encoded with frames costing `cost` bytes in
// a PHROM (PhraseCost()).
core::project::Phrase EncodedPhrase(int n, const std::string& name, int cost);

// A locked phrase with no audio, whose frames cost `cost` bytes.
core::project::Phrase LockedPhrase(int n, const std::string& name, int cost);

// A project holding `phrases`, in order; the test fails if they break the
// project's rules.
core::project::Project ProjectWith(std::vector<core::project::Phrase> phrases);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_PROJECT_BUILDER_H_
