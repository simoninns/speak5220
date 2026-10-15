/*
 * File:        phrase_name_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the phrase name rules and suggested names
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrase_name.h"

#include <array>
#include <set>
#include <string>
#include <string_view>

#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::project {
namespace {

// The Acorn PHROM A word list (app/scripts/phroms/acorn_phrom_a.json):
// each word's name as listed, and as stored in the PHROM.
constexpr auto kAcornNames = std::to_array<std::string_view>({"-0.125",
                                                              "-0.25",
                                                              "(TONE 1)",
                                                              "(TONE2)",
                                                              "-D",
                                                              "-ED",
                                                              "-ING",
                                                              "-S",
                                                              "-TEEN",
                                                              "-TH",
                                                              "-TY",
                                                              "-Z",
                                                              "ZERO (0)",
                                                              "HUNDRED (00)",
                                                              "THOUSAND (000)",
                                                              "ONE,WON",
                                                              "TWO,TO,TOO",
                                                              "2- (TWEN-)",
                                                              "THREE",
                                                              "3- (THIR-)",
                                                              "FOUR,FOR,FORE",
                                                              "4- (FOR-)",
                                                              "FIVE",
                                                              "5- (FIF-)",
                                                              "SIX",
                                                              "6- (SIX-)",
                                                              "SEVEN",
                                                              "7- (SEVEN-)",
                                                              "EIGHT,ATE",
                                                              "8- (EIGHT-)",
                                                              "NINE",
                                                              "9- (NIN-)",
                                                              "A",
                                                              "ACORN",
                                                              "AFTER",
                                                              "AGAIN",
                                                              "AMOUNT",
                                                              "AN",
                                                              "AND",
                                                              "ANOTHER",
                                                              "ANSWER",
                                                              "ANY",
                                                              "AVAILABLE",
                                                              "B,BEE,BE",
                                                              "BAD",
                                                              "BETWEEN",
                                                              "BOTH",
                                                              "BUTTON",
                                                              "C,SEE,SEA",
                                                              "CASSETTE",
                                                              "CHARACTER",
                                                              "COMPLETE",
                                                              "COMPUTER",
                                                              "CORRECT",
                                                              "D",
                                                              "DATA",
                                                              "DATE",
                                                              "DO",
                                                              "DOLLAR",
                                                              "DONT",
                                                              "DOWN",
                                                              "E",
                                                              "EACH",
                                                              "ELEVEN",
                                                              "ENGAGED",
                                                              "ENTER",
                                                              "ERROR",
                                                              "ESCAPE",
                                                              "F",
                                                              "FEW",
                                                              "FILE",
                                                              "FIRST",
                                                              "FOUND",
                                                              "FROM",
                                                              "G",
                                                              "GOOD",
                                                              "H",
                                                              "HAVE",
                                                              "I,EYE",
                                                              "ILLEGAL",
                                                              "IN-",
                                                              "INPUT",
                                                              "IS",
                                                              "J,JAY",
                                                              "K",
                                                              "KEY",
                                                              "L",
                                                              "LARGE",
                                                              "LAST",
                                                              "LINE",
                                                              "M",
                                                              "MANY",
                                                              "MINUS",
                                                              "MORE",
                                                              "MUST",
                                                              "N",
                                                              "NAME",
                                                              "NEGATIVE",
                                                              "NEW",
                                                              "NO,KNOW",
                                                              "NOT,KNOT",
                                                              "NOW",
                                                              "NUMBER",
                                                              "O",
                                                              "O'CLOCK",
                                                              "OF",
                                                              "OFF",
                                                              "OLD",
                                                              "ON",
                                                              "ONLY",
                                                              "OR",
                                                              "P,PEA",
                                                              "PARAMETER",
                                                              "PENCE",
                                                              "PLEASE",
                                                              "PLUS",
                                                              "POINT",
                                                              "POSITIVE",
                                                              "POUN-",
                                                              "PRESS",
                                                              "PROGRAM",
                                                              "Q,QUEUE",
                                                              "R,ARE",
                                                              "RED",
                                                              "RESET",
                                                              "RETURN",
                                                              "RUN",
                                                              "RUNNING",
                                                              "S",
                                                              "SAME",
                                                              "SCORE",
                                                              "SECOND",
                                                              "SMALL",
                                                              "START",
                                                              "STOP",
                                                              "SWITCH",
                                                              "T,TEA,TEE",
                                                              "TEN",
                                                              "THANK",
                                                              "THAT",
                                                              "THE",
                                                              "THEN",
                                                              "THIRD",
                                                              "THIS",
                                                              "TIME",
                                                              "TRY",
                                                              "TWELVE",
                                                              "TYPE",
                                                              "U,YOU",
                                                              "UH",
                                                              "UP",
                                                              "V",
                                                              "VERY",
                                                              "W",
                                                              "WANT",
                                                              "WAS",
                                                              "WERE",
                                                              "WHAT",
                                                              "WHICH",
                                                              "X",
                                                              "Y,WHY",
                                                              "YEAR",
                                                              "YES",
                                                              "YOUR",
                                                              "Z"});
constexpr auto kAcornStoredNames = std::to_array<std::string_view>(
    {"(0.125)",   "(0.25)",   "(TONE1)",   "(TONE2)",  "-D",        "-ED",
     "-ING",      "-S",       "-TEEN",     "-TH",      "-TY",       "-Z",
     "0",         "00",       "000",       "1",        "2",         "2-",
     "3",         "3-",       "4",         "4-",       "5",         "5-",
     "6",         "6-",       "7",         "7-",       "8",         "8-",
     "9",         "9-",       "A",         "ACORN",    "AFTER",     "AGAIN",
     "AMOUNT",    "AN",       "AND",       "ANOTHER",  "ANSWER",    "ANY",
     "AVAILABLE", "B",        "BAD",       "BETWEEN",  "BOTH",      "BUTTON",
     "C",         "CASSETTE", "CHARACTER", "COMPLETE", "COMPUTER",  "CORRECT",
     "D",         "DATA",     "DATE",      "DO",       "DOLLAR",    "DONT",
     "DOWN",      "E",        "EACH",      "ELEVEN",   "ENGAGED",   "ENTER",
     "ERROR",     "ESCAPE",   "F",         "FEW",      "FILE",      "FIRST",
     "FOUND",     "FROM",     "G",         "GOOD",     "H",         "HAVE",
     "I",         "ILLEGAL",  "IN-",       "INPUT",    "IS",        "J",
     "K",         "KEY",      "L",         "LARGE",    "LAST",      "LINE",
     "M",         "MANY",     "MINUS",     "MORE",     "MUST",      "N",
     "NAME",      "NEGATIVE", "NEW",       "NO",       "NOT",       "NOW",
     "NUMBER",    "O",        "O'CLOCK",   "OF",       "OFF",       "OLD",
     "ON",        "ONLY",     "OR",        "P",        "PARAMETER", "PENCE",
     "PLEASE",    "PLUS",     "POINT",     "POSITIVE", "POUN-",     "PRESS",
     "PROGRAM",   "Q",        "R",         "RED",      "RESET",     "RETURN",
     "RUN",       "RUNNING",  "S",         "SAME",     "SCORE",     "SECOND",
     "SMALL",     "START",    "STOP",      "SWITCH",   "T",         "TEN",
     "THANK",     "THAT",     "THE",       "THEN",     "THIRD",     "THIS",
     "TIME",      "TRY",      "TWELVE",    "TYPE",     "U",         "UH",
     "UP",        "V",        "VERY",      "W",        "WANT",      "WAS",
     "WERE",      "WHAT",     "WHICH",     "X",        "Y",         "YEAR",
     "YES",       "YOUR",     "Z"});

TEST(PhraseNameTest, AcceptsEveryNameInTheAcornWordList) {
  for (const std::string_view name : kAcornNames) {
    const StatusOr<std::string> normal = NormalisePhraseName(name);
    ASSERT_TRUE(normal.IsOk()) << name << ": " << normal.GetStatus().ToString();
    EXPECT_EQ(*normal, name);
  }
  for (const std::string_view name : kAcornStoredNames) {
    const StatusOr<std::string> normal = NormalisePhraseName(name);
    ASSERT_TRUE(normal.IsOk()) << name;
    EXPECT_EQ(*normal, name);
  }
}

TEST(PhraseNameTest, ConvertsLowerCaseToUpperCase) {
  EXPECT_EQ(*NormalisePhraseName("hello"), "HELLO");
  EXPECT_EQ(*NormalisePhraseName("o'clock"), "O'CLOCK");
  EXPECT_EQ(*NormalisePhraseName("Two,To,Too"), "TWO,TO,TOO");
}

TEST(PhraseNameTest, RemovesLeadingAndTrailingSpaces) {
  EXPECT_EQ(*NormalisePhraseName("  TONE 1 "), "TONE 1");
}

TEST(PhraseNameTest, RefusesOtherCharactersWithAMessage) {
  for (const char* name :
       {"HELLO!", "A_B", "A/B", "CAF\xc3\x89", "A\tB", "50%", "A:B", "\"Q\""}) {
    const StatusOr<std::string> normal = NormalisePhraseName(name);
    ASSERT_FALSE(normal.IsOk()) << name;
    EXPECT_EQ(normal.GetStatus().Code(), StatusCode::kInvalidArgument);
    EXPECT_NE(normal.GetStatus().Message().find("A-Z"), std::string::npos);
  }
}

TEST(PhraseNameTest, NamesTheRefusedCharacter) {
  EXPECT_NE(NormalisePhraseName("HELLO!").GetStatus().Message().find("\"!\""),
            std::string::npos);
  EXPECT_NE(NormalisePhraseName("A\tB").GetStatus().Message().find("0x09"),
            std::string::npos);
}

TEST(PhraseNameTest, AllowsSixteenCharactersButNoMore) {
  EXPECT_TRUE(NormalisePhraseName("ABCDEFGHIJKLMNOP").IsOk());
  EXPECT_FALSE(NormalisePhraseName("ABCDEFGHIJKLMNOPQ").IsOk());
}

TEST(PhraseNameTest, RefusesAnEmptyName) {
  EXPECT_FALSE(NormalisePhraseName("").IsOk());
  EXPECT_FALSE(NormalisePhraseName("   ").IsOk());
}

TEST(PhraseNameTest, SuggestsANameFromAFileName) {
  EXPECT_EQ(SuggestPhraseName("hello", {}), "HELLO");
  EXPECT_EQ(SuggestPhraseName("good_morning", {}), "GOOD MORNING");
  EXPECT_EQ(SuggestPhraseName("__take 2__", {}), "TAKE 2");
  EXPECT_EQ(SuggestPhraseName("o'clock", {}), "O'CLOCK");
}

TEST(PhraseNameTest, SuggestionIsCutToTheLengthLimit) {
  EXPECT_EQ(SuggestPhraseName("a_very_long_file_name", {}), "A VERY LONG FILE");
  EXPECT_EQ(SuggestPhraseName("abcdefghijklmno pq", {}), "ABCDEFGHIJKLMNO");
}

TEST(PhraseNameTest, SuggestionFallsBackWhenNothingIsUsable) {
  EXPECT_EQ(SuggestPhraseName("___", {}), "PHRASE");
  EXPECT_EQ(SuggestPhraseName("", {}), "PHRASE");
}

TEST(PhraseNameTest, SuggestionAvoidsTakenNames) {
  const std::set<std::string> taken = {"HELLO", "HELLO 2"};
  EXPECT_EQ(SuggestPhraseName("hello", taken), "HELLO 3");
  const std::set<std::string> long_taken = {"ABCDEFGHIJKLMNOP"};
  EXPECT_EQ(SuggestPhraseName("abcdefghijklmnop", long_taken),
            "ABCDEFGHIJKLMN 2");
}

TEST(PhraseNameTest, EverySuggestionIsAValidName) {
  for (const char* stem : {"hello", "x-y.z", "123", "a  b", "..."}) {
    const std::string name = SuggestPhraseName(stem, {});
    EXPECT_EQ(NormalisePhraseName(name).IsOk(), true) << name;
  }
}

}  // namespace
}  // namespace speak5220::core::project
