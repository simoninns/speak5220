/*
 * File:        encoder_options_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the encoder options: defaults, metadata and
 *              range checks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/encoder_options.h"

#include <cmath>
#include <limits>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

// tms5220enc.py DEFAULTS, but for four chosen on the corpus (HLD §9).
TEST(EncoderOptionsTest, DefaultsAreTheReferencesButForFourChosenOnes) {
  const EncoderOptions o;
  EXPECT_EQ(o.target_rms, 24.0);
  EXPECT_EQ(o.repeat_lambda, 70.0);
  EXPECT_EQ(o.gate_db, 45.0);
  EXPECT_EQ(o.pre_emphasis, PreEmphasis::Chirp());
  EXPECT_EQ(o.window, 256);
  EXPECT_EQ(o.lag_window_hz, 60.0);
  EXPECT_EQ(o.voicing_threshold, 0.3);
  EXPECT_EQ(o.voicing_evidence, 5.0);
  EXPECT_EQ(o.level_agc, 1.25);
}

// The reference's options that made no difference on the corpus, or
// repeated another option, are fixed in the encoder (HLD §8, "Options"),
// so a project that still lists one is told it is not an option.
TEST(EncoderOptionsTest, FixedOptionsAreNotOptions) {
  EncoderOptions o;
  for (const std::string_view field :
       {"target_peak", "peak_limit", "repeats", "iterations", "fade_unvoiced",
        "quiet_pauses", "compression"}) {
    EXPECT_EQ(FindEncoderOption(field), nullptr) << field;
    const Status status = SetEncoderOption(o, field, "1");
    EXPECT_EQ(status.Code(), StatusCode::kInvalidArgument) << field;
    EXPECT_NE(status.Message().find("unknown encoder option"),
              std::string::npos)
        << field;
  }
}

// The resonance hold is not in the reference encoder either, and is on
// by default: it removes pops a high voice can give (HLD §9).
TEST(EncoderOptionsTest, ResonanceHoldIsOnByDefault) {
  EXPECT_EQ(EncoderOptions{}.resonance_hold_db, 14.0);
  EncoderOptions o;
  ASSERT_TRUE(SetEncoderOption(o, "resonance_hold_db", "none").IsOk());
  EXPECT_FALSE(o.resonance_hold_db.has_value());
  EXPECT_EQ(EncoderOptionText(o, "resonance_hold_db"), "none");
  ASSERT_TRUE(SetEncoderOption(o, "resonance_hold_db", "20").IsOk());
  EXPECT_EQ(o.resonance_hold_db, 20.0);
  EXPECT_FALSE(SetEncoderOption(o, "resonance_hold_db", "1").IsOk());
}

// The squelch is not in the reference encoder, and is off by default: it
// did not raise the score on the corpus (HLD §9).
TEST(EncoderOptionsTest, SquelchIsOffByDefault) {
  EXPECT_FALSE(EncoderOptions{}.squelch_db.has_value());
}

// The GUI shows default_text and resets to EncoderOptions{}, so they must
// agree; and a default lies on its slider's steps, so the slider can
// return to it.
TEST(EncoderOptionsTest, DefaultTextIsTheDefaultOnTheSlidersSteps) {
  const EncoderOptions o;
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    const std::string text = EncoderOptionText(o, info.field);
    EXPECT_EQ(info.default_text.empty() ? std::string(kUnsetOptionText)
                                        : std::string(info.default_text),
              text)
        << info.field;
    if (info.type == EncoderOptionType::kReal ||
        info.type == EncoderOptionType::kInteger ||
        (info.type == EncoderOptionType::kOptionalReal &&
         !info.default_text.empty())) {
      const double value = std::stod(text);
      const double steps = (value - info.typical_minimum) / info.step;
      EXPECT_NEAR(steps, std::round(steps), 1e-9) << info.field;
    }
  }
}

TEST(EncoderOptionsTest, DefaultsAreValid) {
  EXPECT_TRUE(ValidateEncoderOptions({}).IsOk());
}

TEST(EncoderOptionsTest, InfoListsEveryOptionOnceWithItsDefaultInRange) {
  std::set<std::string_view> keys;
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    EXPECT_TRUE(keys.insert(info.key).second) << info.key;
    EXPECT_FALSE(info.label.empty()) << info.key;
    EXPECT_FALSE(info.help.empty()) << info.key;
    EXPECT_LE(info.minimum, info.maximum) << info.key;
  }
  EXPECT_EQ(keys.size(), 11U);
  EXPECT_EQ(EncoderOptionInfos().front().key, "targetRms");
  EXPECT_EQ(EncoderOptionInfos().back().key, "levelAgc");
}

TEST(EncoderOptionsTest, BasicOptionsAreTheOnesWithTheClearestEffect) {
  std::vector<std::string_view> basic;
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    if (!info.advanced) {
      basic.push_back(info.field);
    }
  }
  EXPECT_EQ(basic, (std::vector<std::string_view>{"target_rms", "repeat_lambda",
                                                  "squelch_db", "pre_emphasis",
                                                  "voicing_threshold"}));
}

TEST(EncoderOptionsTest, TypicalRangesLieWithinTheValidRanges) {
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    EXPECT_GE(info.typical_minimum, info.minimum) << info.field;
    EXPECT_LE(info.typical_maximum, info.maximum) << info.field;
    EXPECT_LT(info.typical_minimum, info.typical_maximum) << info.field;
    EXPECT_GT(info.step, 0.0) << info.field;
  }
}

TEST(EncoderOptionsTest, RejectsValuesOutsideTheirRange) {
  EncoderOptions o;
  o.target_rms = 0.5;
  EXPECT_EQ(ValidateEncoderOptions(o).Code(), StatusCode::kOutOfRange);
  o = {};
  o.window = 1000;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
  o = {};
  o.level_agc = 0.5;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
  o = {};
  o.gate_db = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
}

TEST(EncoderOptionsTest, ChecksSquelchOnlyWhenSet) {
  EncoderOptions o;
  o.squelch_db = 30.0;
  EXPECT_TRUE(ValidateEncoderOptions(o).IsOk());
  o.squelch_db = 2.0;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
  EXPECT_FALSE(SetEncoderOption(o, "squelch_db", "100").IsOk());
}

TEST(EncoderOptionsTest, SetsAndReadsTheSquelchAsText) {
  EncoderOptions o;
  EXPECT_EQ(EncoderOptionText(o, "squelch_db"), "none");
  ASSERT_TRUE(SetEncoderOption(o, "squelch_db", "24").IsOk());
  EXPECT_EQ(o.squelch_db, 24.0);
  EXPECT_EQ(o.resonance_hold_db, 14.0);
  EXPECT_EQ(EncoderOptionText(o, "squelch_db"), "24");
  ASSERT_TRUE(SetEncoderOption(o, "squelch_db", "none").IsOk());
  EXPECT_FALSE(o.squelch_db.has_value());
}

TEST(EncoderOptionsTest, ChecksPreEmphasisCoefficientOnlyWhenNotChirp) {
  EncoderOptions o;
  o.pre_emphasis = PreEmphasis::FirstOrder(0.9);
  EXPECT_TRUE(ValidateEncoderOptions(o).IsOk());
  o.pre_emphasis = PreEmphasis::FirstOrder(1.5);
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
}

TEST(EncoderOptionsTest, ErrorNamesTheOption) {
  EncoderOptions o;
  o.repeat_lambda = -1.0;
  EXPECT_NE(ValidateEncoderOptions(o).Message().find("repeat_lambda"),
            std::string::npos);
}

TEST(EncoderOptionsTest, EveryOptionHasASnakeCaseField) {
  std::set<std::string_view> fields;
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    EXPECT_TRUE(fields.insert(info.field).second) << info.field;
    for (const char c : info.field) {
      EXPECT_TRUE((c >= 'a' && c <= 'z') || c == '_') << info.field;
    }
    EXPECT_EQ(FindEncoderOption(info.field), &info);
  }
  EXPECT_EQ(FindEncoderOption("targetRms"), nullptr);
}

TEST(EncoderOptionsTest, TextOfEveryDefaultReadsBackToTheDefaults) {
  const EncoderOptions defaults;
  EncoderOptions options;
  options.target_rms = 1.0;
  options.pre_emphasis = PreEmphasis::FirstOrder(0.5);
  options.resonance_hold_db.reset();
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    const std::string text = EncoderOptionText(defaults, info.field);
    EXPECT_TRUE(SetEncoderOption(options, info.field, text).IsOk())
        << info.field << " = " << text;
  }
  EXPECT_EQ(options, defaults);
}

TEST(EncoderOptionsTest, TextOfEachTypeIsWhatAUserTypes) {
  EncoderOptions options;
  EXPECT_EQ(EncoderOptionText(options, "target_rms"), "24");
  EXPECT_EQ(EncoderOptionText(options, "voicing_threshold"), "0.3");
  EXPECT_EQ(EncoderOptionText(options, "window"), "256");
  EXPECT_EQ(EncoderOptionText(options, "squelch_db"), "none");
  EXPECT_EQ(EncoderOptionText(options, "pre_emphasis"), "chirp");
  options.squelch_db = 30.0;
  options.pre_emphasis = PreEmphasis::FirstOrder(0.9375);
  EXPECT_EQ(EncoderOptionText(options, "squelch_db"), "30");
  EXPECT_EQ(EncoderOptionText(options, "pre_emphasis"), "0.9375");
  EXPECT_EQ(EncoderOptionText(options, "no_such_option"), "");
}

TEST(EncoderOptionsTest, SetsEachTypeFromText) {
  EncoderOptions options;
  ASSERT_TRUE(SetEncoderOption(options, "target_rms", "18.5").IsOk());
  EXPECT_EQ(options.target_rms, 18.5);
  ASSERT_TRUE(SetEncoderOption(options, "window", "200").IsOk());
  EXPECT_EQ(options.window, 200);
  ASSERT_TRUE(SetEncoderOption(options, "squelch_db", "40").IsOk());
  EXPECT_EQ(options.squelch_db, 40.0);
  ASSERT_TRUE(SetEncoderOption(options, "squelch_db", "none").IsOk());
  EXPECT_FALSE(options.squelch_db.has_value());
  ASSERT_TRUE(SetEncoderOption(options, "pre_emphasis", "0.9").IsOk());
  EXPECT_EQ(options.pre_emphasis, PreEmphasis::FirstOrder(0.9));
  ASSERT_TRUE(SetEncoderOption(options, "pre_emphasis", "chirp").IsOk());
  EXPECT_EQ(options.pre_emphasis, PreEmphasis::Chirp());
}

TEST(EncoderOptionsTest, RefusingTextLeavesTheOptionsUnchanged) {
  const EncoderOptions defaults;
  EncoderOptions options;
  const std::vector<std::pair<std::string, std::string>> bad = {
      {"target_rms", "loud"}, {"target_rms", "1,5"},   {"target_rms", "500"},
      {"window", "240.5"},    {"window", "10"},        {"squelch_db", "x"},
      {"squelch_db", "true"}, {"pre_emphasis", "1.5"}, {"pre_emphasis", "fir"},
      {"targetRms", "20"},    {"no_such_option", "1"},
  };
  for (const auto& [field, text] : bad) {
    EXPECT_FALSE(SetEncoderOption(options, field, text).IsOk())
        << field << " = " << text;
    EXPECT_EQ(options, defaults) << field;
  }
}

TEST(EncoderOptionsTest, RefusalNamesTheOptionByItsField) {
  EncoderOptions options;
  EXPECT_NE(SetEncoderOption(options, "gate_db", "5").Message().find("gate_db"),
            std::string::npos);
  EXPECT_NE(
      SetEncoderOption(options, "gate_db", "x").Message().find("a number"),
      std::string::npos);
}

}  // namespace
}  // namespace speak5220::core::encoder
