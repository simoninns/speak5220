/*
 * File:        number_text_test.cpp
 * Module:      util tests
 * Purpose:     Unit tests of the locale-independent number conversions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/number_text.h"

#include <cstdint>
#include <limits>
#include <optional>

#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

TEST(NumberTextTest, FormatsTheShortestExactText) {
  EXPECT_EQ(FormatNumber(20.0), "20");
  EXPECT_EQ(FormatNumber(0.3), "0.3");
  EXPECT_EQ(FormatNumber(-0.5), "-0.5");
  EXPECT_EQ(FormatNumber(2.322), "2.322");
  EXPECT_EQ(FormatNumber(1e-9), "1e-09");
}

TEST(NumberTextTest, FormattedNumbersReadBackExactly) {
  for (const double value : {0.1, 1.0 / 3.0, 2.0e-300, 123456.789, -7.25,
                             std::numeric_limits<double>::max()}) {
    const std::optional<double> read = ParseNumber(FormatNumber(value));
    ASSERT_TRUE(read.has_value()) << FormatNumber(value);
    EXPECT_EQ(*read, value);
  }
}

TEST(NumberTextTest, ParsesDecimalNumbers) {
  EXPECT_EQ(ParseNumber("18"), 18.0);
  EXPECT_EQ(ParseNumber("0.35"), 0.35);
  EXPECT_EQ(ParseNumber("-2.5e3"), -2500.0);
}

TEST(NumberTextTest, RefusesTextThatIsNotWhollyANumber) {
  for (const char* text : {"", " 1", "1 ", "1,5", "nan", "inf", "0x10", "1e",
                           "abc", "--1", "1.2.3", "1e999"}) {
    EXPECT_FALSE(ParseNumber(text).has_value()) << text;
  }
}

TEST(NumberTextTest, ParsesWholeNumbers) {
  EXPECT_EQ(ParseInteger("240"), 240);
  EXPECT_EQ(ParseInteger("-3"), -3);
  EXPECT_EQ(ParseInteger("9223372036854775807"),
            std::numeric_limits<std::int64_t>::max());
}

TEST(NumberTextTest, RefusesTextThatIsNotAWholeNumber) {
  for (const char* text :
       {"", "1.0", "1e3", " 1", "+1x", "9223372036854775808", "0x1"}) {
    EXPECT_FALSE(ParseInteger(text).has_value()) << text;
  }
}

}  // namespace
}  // namespace speak5220::core
