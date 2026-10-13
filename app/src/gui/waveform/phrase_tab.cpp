/*
 * File:        phrase_tab.cpp
 * Module:      waveform
 * Purpose:     The phrase editor's tabs, each one waveform with its
 *              controls, and their keys for remembering the one chosen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/phrase_tab.h"

#include <optional>
#include <string_view>

namespace speak5220::app {

std::string_view PhraseTabKey(PhraseTab tab) {
  switch (tab) {
    case PhraseTab::kOriginal:
      return "original";
    case PhraseTab::kPreprocess:
      return "preprocess";
    case PhraseTab::kEncode:
      return "encode";
  }
  return "original";
}

std::optional<PhraseTab> PhraseTabFromKey(std::string_view key) {
  for (const PhraseTab tab : kPhraseTabs) {
    if (PhraseTabKey(tab) == key) {
      return tab;
    }
  }
  return std::nullopt;
}

}  // namespace speak5220::app
