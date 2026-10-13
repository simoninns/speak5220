/*
 * File:        phrase_tab.h
 * Module:      waveform
 * Purpose:     The phrase editor's tabs, each one waveform with its
 *              controls, and their keys for remembering the one chosen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_PHRASE_TAB_H_
#define SPEAK5220_GUI_WAVEFORM_PHRASE_TAB_H_

#include <array>
#include <optional>
#include <string_view>

namespace speak5220::app {

// The phrase editor's tabs, in their order: the original recording, the
// recording preprocessed with the preprocessing settings, and the encoded
// audio with the encoder panel.
enum class PhraseTab { kOriginal, kPreprocess, kEncode };

// Every tab, in its order.
inline constexpr std::array<PhraseTab, 3> kPhraseTabs = {
    PhraseTab::kOriginal, PhraseTab::kPreprocess, PhraseTab::kEncode};

// The tab's key for storing which one was chosen ("original",
// "preprocess", "encode"). Pure.
std::string_view PhraseTabKey(PhraseTab tab);
// The tab with `key`, or none for a key no tab has. Pure.
std::optional<PhraseTab> PhraseTabFromKey(std::string_view key);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_PHRASE_TAB_H_
