/*
 * File:        phrase_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of a phrase's encoder overrides: applying them
 *              over the defaults, and finding them for given options
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrase.h"

#include "core/encoder/encoder_options.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::project {
namespace {

TEST(PhraseTest, OverridesForEqualOptionsAreEmpty) {
  const encoder::EncoderOptions defaults;
  EXPECT_TRUE(OverridesFor(defaults, defaults).empty());
}

TEST(PhraseTest, OverridesForNameOnlyTheOptionsThatDiffer) {
  const encoder::EncoderOptions defaults;
  encoder::EncoderOptions options = defaults;
  options.target_rms = 18.5;
  options.pre_emphasis = encoder::PreEmphasis::FirstOrder(0.9);
  options.target_peak = 100.0;
  const EncoderOverrides overrides = OverridesFor(defaults, options);
  EXPECT_EQ(overrides, (EncoderOverrides{{"pre_emphasis", "0.9"},
                                         {"target_peak", "100"},
                                         {"target_rms", "18.5"}}));
}

TEST(PhraseTest, OverridesForApplyBackToTheSameOptions) {
  encoder::EncoderOptions defaults;
  defaults.repeats = false;
  defaults.target_peak = 90.0;
  encoder::EncoderOptions options;
  options.voicing_threshold = 0.35;
  const StatusOr<encoder::EncoderOptions> applied =
      EffectiveEncoderOptions(defaults, OverridesFor(defaults, options));
  ASSERT_TRUE(applied.IsOk());
  EXPECT_EQ(*applied, options);
}

}  // namespace
}  // namespace speak5220::core::project
