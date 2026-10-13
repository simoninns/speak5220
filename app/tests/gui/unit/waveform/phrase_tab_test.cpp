/*
 * File:        phrase_tab_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of the phrase editor tabs' keys
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/phrase_tab.h"

#include <optional>

#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

TEST(PhraseTabTest, EveryTabIsFoundByItsKey) {
  for (const PhraseTab tab : kPhraseTabs) {
    EXPECT_EQ(PhraseTabFromKey(PhraseTabKey(tab)), tab);
  }
  EXPECT_EQ(PhraseTabKey(PhraseTab::kPreprocess), "preprocess");
}

TEST(PhraseTabTest, UnknownKeyIsNoTab) {
  EXPECT_EQ(PhraseTabFromKey(""), std::nullopt);
  EXPECT_EQ(PhraseTabFromKey("Encode"), std::nullopt);
}

}  // namespace
}  // namespace speak5220::app
