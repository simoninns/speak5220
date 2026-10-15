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

#include "core/preprocess/preprocess.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

// tms5220enc.py DEFAULTS, but for five chosen on the corpus and Acorn's
// PHROM.
TEST(EncoderOptionsTest, DefaultsAreTheReferencesButForFiveChosenOnes) {
  const EncoderOptions o;
  EXPECT_EQ(o.target_rms, 20.0);
  EXPECT_EQ(o.repeat_lambda, 300.0);
  EXPECT_EQ(o.gate_db, 45.0);
  EXPECT_EQ(o.window, 256);
  EXPECT_EQ(o.lag_window_hz, 60.0);
  EXPECT_EQ(o.voicing_threshold, 0.6);
  EXPECT_EQ(o.voicing_evidence, 5.0);
  EXPECT_EQ(o.level_agc, 1.25);
}

// Not in the reference encoder: short voicing runs are smoothed by
// default, and there is no preprocessing.
TEST(EncoderOptionsTest, VoicingRunsAreSmoothedAndNothingIsPreprocessed) {
  const EncoderOptions o;
  EXPECT_EQ(o.voicing_min_run, 2);
  EXPECT_EQ(o.preprocess, preprocess::PreprocessOptions{});
}

// The options whose value can turn them off name it, for the GUI to show
// as "off".
TEST(EncoderOptionsTest, OptionsWithAnOffValueNameIt) {
  std::vector<std::string_view> named;
  EncoderOptions off;
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    if (!info.off_text.empty()) {
      named.push_back(info.field);
      ASSERT_TRUE(SetEncoderOption(off, info.field, info.off_text).IsOk())
          << info.field;
    }
  }
  EXPECT_EQ(named,
            (std::vector<std::string_view>{"treble_tilt", "voicing_min_run"}));
  EXPECT_EQ(off.preprocess.treble_tilt, 0.0);
  EXPECT_EQ(off.voicing_min_run, 1);
}

// The noise gate and its speed, the bass cut and the treble tilt are the
// preprocessing's, listed first; the rest are the encoder's.
TEST(EncoderOptionsTest, PreprocessingOptionsComeFirst) {
  std::vector<std::string_view> preprocessing;
  bool encoder_seen = false;
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    if (info.stage == OptionStage::kPreprocess) {
      EXPECT_FALSE(encoder_seen) << info.field;
      EXPECT_FALSE(info.advanced) << info.field;
      preprocessing.push_back(info.field);
    } else {
      encoder_seen = true;
    }
  }
  EXPECT_EQ(preprocessing, (std::vector<std::string_view>{
                               "noise_gate_db", "noise_gate_ms", "bass_cut_hz",
                               "treble_tilt", "de_ess_db"}));
}

// Each preprocessing option sets its field of EncoderOptions::preprocess.
TEST(EncoderOptionsTest, PreprocessingOptionsSetThePreprocessing) {
  EncoderOptions o;
  ASSERT_TRUE(SetEncoderOption(o, "noise_gate_db", "30").IsOk());
  ASSERT_TRUE(SetEncoderOption(o, "noise_gate_ms", "250").IsOk());
  ASSERT_TRUE(SetEncoderOption(o, "bass_cut_hz", "200").IsOk());
  ASSERT_TRUE(SetEncoderOption(o, "treble_tilt", "0.4").IsOk());
  ASSERT_TRUE(SetEncoderOption(o, "de_ess_db", "9").IsOk());
  EXPECT_EQ(o.preprocess, (preprocess::PreprocessOptions{.noise_gate_db = 30.0,
                                                         .noise_gate_ms = 250.0,
                                                         .bass_cut_hz = 200.0,
                                                         .treble_tilt = 0.4,
                                                         .de_ess_db = 9.0}));
  EXPECT_EQ(EncoderOptionText(o, "noise_gate_ms"), "250");
  EXPECT_EQ(EncoderOptionText(o, "bass_cut_hz"), "200");
  EXPECT_EQ(EncoderOptionText(o, "treble_tilt"), "0.4");
}

// Every preset is valid.
TEST(EncoderOptionsTest, PreprocessPresetsAreValid) {
  for (const preprocess::PreprocessPreset& preset :
       preprocess::PreprocessPresets()) {
    EncoderOptions o;
    o.preprocess = preset.options;
    EXPECT_TRUE(ValidateEncoderOptions(o).IsOk()) << preset.name;
  }
}

TEST(EncoderOptionsTest, NewOptionsAreCheckedAgainstTheirRanges) {
  EncoderOptions o;
  EXPECT_TRUE(SetEncoderOption(o, "bass_cut_hz", "200").IsOk());
  EXPECT_EQ(o.preprocess.bass_cut_hz, 200.0);
  EXPECT_FALSE(SetEncoderOption(o, "bass_cut_hz", "20").IsOk());
  EXPECT_TRUE(SetEncoderOption(o, "treble_tilt", "0.4").IsOk());
  EXPECT_EQ(o.preprocess.treble_tilt, 0.4);
  EXPECT_FALSE(SetEncoderOption(o, "treble_tilt", "0.99").IsOk());
  EXPECT_TRUE(SetEncoderOption(o, "voicing_min_run", "4").IsOk());
  EXPECT_EQ(o.voicing_min_run, 4);
  EXPECT_FALSE(SetEncoderOption(o, "voicing_min_run", "0").IsOk());
  EXPECT_FALSE(SetEncoderOption(o, "voicing_min_run", "2.5").IsOk());
}

// The reference's options that made no difference on the corpus, or
// repeated another option, are fixed in the encoder,
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
// by default: it removes pops a high voice can give.
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

// The noise gate is not in the reference encoder, and is off by default:
// what it mutes is the user's choice. Its speed has a default, and
// the GUI disables it while the gate is off; it is the only option that
// needs another.
TEST(EncoderOptionsTest, NoiseGateIsOffByDefault) {
  EXPECT_FALSE(EncoderOptions{}.preprocess.noise_gate_db.has_value());
  EXPECT_EQ(EncoderOptions{}.preprocess.noise_gate_ms, 100.0);
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    EXPECT_EQ(info.enabled_by,
              info.field == "noise_gate_ms" ? "noise_gate_db" : "")
        << info.field;
  }
}

TEST(EncoderOptionsTest, DeEsserIsOffByDefault) {
  EXPECT_FALSE(EncoderOptions{}.preprocess.de_ess_db.has_value());
  EXPECT_EQ(EncoderOptionText(EncoderOptions{}, "de_ess_db"), "none");
}

TEST(EncoderOptionsTest, ChecksTheDeEsser) {
  EncoderOptions o;
  EXPECT_FALSE(SetEncoderOption(o, "de_ess_db", "0").IsOk());
  EXPECT_FALSE(SetEncoderOption(o, "de_ess_db", "31").IsOk());
  ASSERT_TRUE(SetEncoderOption(o, "de_ess_db", "9").IsOk());
  ASSERT_TRUE(SetEncoderOption(o, "de_ess_db", "none").IsOk());
  EXPECT_FALSE(o.preprocess.de_ess_db.has_value());
}

TEST(EncoderOptionsTest, ChecksTheNoiseGatesSpeed) {
  EncoderOptions o;
  EXPECT_FALSE(SetEncoderOption(o, "noise_gate_ms", "1").IsOk());
  EXPECT_FALSE(SetEncoderOption(o, "noise_gate_ms", "none").IsOk());
  ASSERT_TRUE(SetEncoderOption(o, "noise_gate_ms", "2000").IsOk());
  EXPECT_EQ(o.preprocess.noise_gate_ms, 2000.0);
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
  EXPECT_EQ(keys.size(), 15U);
  EXPECT_EQ(EncoderOptionInfos().front().key, "noiseGateDb");
  EXPECT_EQ(EncoderOptionInfos().back().key, "levelAgc");
}

TEST(EncoderOptionsTest, BasicOptionsAreTheOnesWithTheClearestEffect) {
  std::vector<std::string_view> basic;
  for (const EncoderOptionInfo& info : EncoderOptionInfos()) {
    if (info.stage == OptionStage::kEncoder && !info.advanced) {
      basic.push_back(info.field);
    }
  }
  EXPECT_EQ(basic, (std::vector<std::string_view>{"target_rms", "repeat_lambda",
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

TEST(EncoderOptionsTest, ChecksNoiseGateOnlyWhenSet) {
  EncoderOptions o;
  o.preprocess.noise_gate_db = 30.0;
  EXPECT_TRUE(ValidateEncoderOptions(o).IsOk());
  o.preprocess.noise_gate_db = 2.0;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
  EXPECT_FALSE(SetEncoderOption(o, "noise_gate_db", "100").IsOk());
}

TEST(EncoderOptionsTest, SetsAndReadsTheNoiseGateAsText) {
  EncoderOptions o;
  EXPECT_EQ(EncoderOptionText(o, "noise_gate_db"), "none");
  ASSERT_TRUE(SetEncoderOption(o, "noise_gate_db", "24").IsOk());
  EXPECT_EQ(o.preprocess.noise_gate_db, 24.0);
  EXPECT_EQ(o.resonance_hold_db, 14.0);
  EXPECT_EQ(EncoderOptionText(o, "noise_gate_db"), "24");
  ASSERT_TRUE(SetEncoderOption(o, "noise_gate_db", "none").IsOk());
  EXPECT_FALSE(o.preprocess.noise_gate_db.has_value());
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
  options.window = 200;
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
  EXPECT_EQ(EncoderOptionText(options, "target_rms"), "20");
  EXPECT_EQ(EncoderOptionText(options, "voicing_threshold"), "0.6");
  EXPECT_EQ(EncoderOptionText(options, "window"), "256");
  EXPECT_EQ(EncoderOptionText(options, "noise_gate_db"), "none");
  options.preprocess.noise_gate_db = 30.0;
  EXPECT_EQ(EncoderOptionText(options, "noise_gate_db"), "30");
  EXPECT_EQ(EncoderOptionText(options, "no_such_option"), "");
}

TEST(EncoderOptionsTest, SetsEachTypeFromText) {
  EncoderOptions options;
  ASSERT_TRUE(SetEncoderOption(options, "target_rms", "18.5").IsOk());
  EXPECT_EQ(options.target_rms, 18.5);
  ASSERT_TRUE(SetEncoderOption(options, "window", "200").IsOk());
  EXPECT_EQ(options.window, 200);
  ASSERT_TRUE(SetEncoderOption(options, "noise_gate_db", "40").IsOk());
  EXPECT_EQ(options.preprocess.noise_gate_db, 40.0);
  ASSERT_TRUE(SetEncoderOption(options, "noise_gate_db", "none").IsOk());
  EXPECT_FALSE(options.preprocess.noise_gate_db.has_value());
}

TEST(EncoderOptionsTest, RefusingTextLeavesTheOptionsUnchanged) {
  const EncoderOptions defaults;
  EncoderOptions options;
  const std::vector<std::pair<std::string, std::string>> bad = {
      {"target_rms", "loud"},    {"target_rms", "1,5"}, {"target_rms", "500"},
      {"window", "240.5"},       {"window", "10"},      {"noise_gate_db", "x"},
      {"noise_gate_db", "true"}, {"targetRms", "20"},   {"no_such_option", "1"},
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
