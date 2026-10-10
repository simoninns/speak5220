/*
 * File:        encoder_options_test.cc
 * Module:      encoder tests
 * Purpose:     Unit tests of the encoder options: defaults, metadata and
 *              range checks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/encoder_options.h"

#include <limits>
#include <set>
#include <string>
#include <string_view>

#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

// tms5220enc.py DEFAULTS.
TEST(EncoderOptionsTest, DefaultsMatchReferenceEncoder) {
  const EncoderOptions o;
  EXPECT_EQ(o.target_rms, 20.0);
  EXPECT_FALSE(o.target_peak.has_value());
  EXPECT_EQ(o.peak_limit, 120.0);
  EXPECT_TRUE(o.repeats);
  EXPECT_EQ(o.repeat_lambda, 70.0);
  EXPECT_EQ(o.gate_db, 45.0);
  EXPECT_EQ(o.pre_emphasis, PreEmphasis::Chirp());
  EXPECT_EQ(o.window, 240);
  EXPECT_EQ(o.lag_window_hz, 60.0);
  EXPECT_EQ(o.voicing_threshold, 0.3);
  EXPECT_EQ(o.voicing_evidence, 4.0);
  EXPECT_EQ(o.iterations, 5);
  EXPECT_TRUE(o.fade_unvoiced);
  EXPECT_EQ(o.level_agc, 1.0);
  EXPECT_EQ(o.compression, 1.0);
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
  EXPECT_EQ(EncoderOptionInfos().front().key, "targetRms");
  EXPECT_EQ(EncoderOptionInfos().back().key, "compression");
}

TEST(EncoderOptionsTest, RejectsValuesOutsideTheirRange) {
  EncoderOptions o;
  o.target_rms = 0.5;
  EXPECT_EQ(ValidateEncoderOptions(o).Code(), StatusCode::kOutOfRange);
  o = {};
  o.window = 1000;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
  o = {};
  o.iterations = 0;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
  o = {};
  o.compression = 0.5;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
  o = {};
  o.gate_db = std::numeric_limits<double>::quiet_NaN();
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
}

TEST(EncoderOptionsTest, ChecksTargetPeakOnlyWhenSet) {
  EncoderOptions o;
  o.target_peak = 100.0;
  EXPECT_TRUE(ValidateEncoderOptions(o).IsOk());
  o.target_peak = 200.0;
  EXPECT_FALSE(ValidateEncoderOptions(o).IsOk());
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
  EXPECT_NE(ValidateEncoderOptions(o).Message().find("repeatLambda"),
            std::string::npos);
}

}  // namespace
}  // namespace speak5220::core::encoder
