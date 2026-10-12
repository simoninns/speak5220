/*
 * File:        c_header_test.cpp
 * Module:      packaging tests
 * Purpose:     Unit tests of the C header exports: the PHROM and Speak
 *              External forms, their names, comment and checks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/c_header.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/packaging/phrom_image.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::packaging {
namespace {

CHeaderInfo Info() {
  return {.prefix = "speech", .project_name = "My PHROM", .licence = "MIT"};
}

// Two small images: words ONE,WON and 2- (TWEN-) in the first, -ING in the
// second. Only the table and a few bytes matter here.
std::vector<PhromImage> Images() {
  PhromImage first;
  first.bytes.assign(kPhromSize, 0);
  first.bytes[0] = 0xAB;
  first.bytes[kPhromSize - 1] = 0x01;
  first.table = {{.number = 127,
                  .name = "ONE,WON",
                  .offset = 0x123,
                  .bytes = 40,
                  .frames = 9},
                 {.number = 128,
                  .name = "2- (TWEN-)",
                  .offset = 0x200,
                  .bytes = 7,
                  .frames = 3}};
  PhromImage second;
  second.bytes.assign(kPhromSize, 0xFF);
  second.table = {{.number = 127,
                   .name = "-ING",
                   .offset = 0x101,
                   .bytes = 5,
                   .frames = 2}};
  return {first, second};
}

std::vector<CHeaderName> Names() {
  return {{.name = "ONE,WON", .identifier = "ONE_WON"},
          {.name = "2- (TWEN-)", .identifier = "W_2_TWEN"},
          {.name = "-ING", .identifier = "ING"}};
}

// The bytes of the array `name` in `text`, read back from its 0x.. values.
std::vector<std::uint8_t> ArrayBytes(const std::string& text,
                                     std::string_view name) {
  std::vector<std::uint8_t> bytes;
  std::size_t at = text.find(std::string(name) + "[");
  if (at == std::string::npos) {
    return bytes;
  }
  at = text.find("= {", at);
  const std::size_t end = text.find("};", at);
  while ((at = text.find("0x", at)) < end) {
    bytes.push_back(static_cast<std::uint8_t>(
        std::stoi(text.substr(at + 2, 2), nullptr, 16)));
    at += 4;
  }
  return bytes;
}

bool Contains(const std::string& text, std::string_view part) {
  return text.find(part) != std::string::npos;
}

TEST(CHeaderTest, PhromArraysHoldTheImagesBytes) {
  const std::vector<PhromImage> images = Images();
  const StatusOr<std::string> text =
      CHeader::ForPhroms(Info(), images, Names());
  ASSERT_TRUE(text.IsOk()) << text.GetStatus().ToString();
  EXPECT_EQ(ArrayBytes(*text, "speech_phrom_1"), images[0].bytes);
  EXPECT_EQ(ArrayBytes(*text, "speech_phrom_2"), images[1].bytes);
  EXPECT_TRUE(Contains(*text, "#define SPEECH_PHROMS 2\n"));
  EXPECT_TRUE(Contains(*text, "#define SPEECH_PHROM_SIZE 16384\n"));
  EXPECT_TRUE(Contains(*text, "#define SPEECH_WORDS 3\n"));
}

TEST(CHeaderTest, PhromTableGivesEachWordsPlace) {
  const StatusOr<std::string> text =
      CHeader::ForPhroms(Info(), Images(), Names());
  ASSERT_TRUE(text.IsOk());
  EXPECT_TRUE(Contains(*text,
                       "  SPEECH_WORD_ONE_WON,\n  SPEECH_WORD_W_2_TWEN,\n"
                       "  SPEECH_WORD_ING,\n};"));
  EXPECT_TRUE(Contains(
      *text,
      "  {1, 127, 0x0123, 40, \"ONE,WON\"},  /* SPEECH_WORD_ONE_WON */\n"));
  EXPECT_TRUE(Contains(*text, "  {1, 128, 0x0200, 7, \"2- (TWEN-)\"},"));
  EXPECT_TRUE(Contains(*text, "  {2, 127, 0x0101, 5, \"-ING\"},"));
}

TEST(CHeaderTest, CommentHasTheLicenceProjectAndNameMapping) {
  const StatusOr<std::string> text =
      CHeader::ForPhroms(Info(), Images(), Names());
  ASSERT_TRUE(text.IsOk());
  EXPECT_EQ(text->rfind("/*\n * SPDX-License-Identifier: MIT\n", 0), 0U);
  EXPECT_TRUE(Contains(*text, "project \"My PHROM\""));
  EXPECT_TRUE(Contains(*text, " *   ONE,WON     SPEECH_WORD_ONE_WON\n"));
  EXPECT_TRUE(Contains(*text, " *   2- (TWEN-)  SPEECH_WORD_W_2_TWEN\n"));
  EXPECT_TRUE(Contains(*text, "#ifndef SPEECH_H_\n#define SPEECH_H_\n"));
  EXPECT_TRUE(Contains(*text, "#include <stdint.h>\n"));
  EXPECT_TRUE(text->ends_with("#endif  /* SPEECH_H_ */\n"));
}

TEST(CHeaderTest, SameInputGivesTheSameText) {
  EXPECT_EQ(CHeader::ForPhroms(Info(), Images(), Names()).Value(),
            CHeader::ForPhroms(Info(), Images(), Names()).Value());
}

TEST(CHeaderTest, ProjectNameCannotEndTheComment) {
  CHeaderInfo info = Info();
  info.project_name = "a */ b\nc";
  const StatusOr<std::string> text = CHeader::ForStreams(info, {});
  ASSERT_TRUE(text.IsOk());
  EXPECT_TRUE(Contains(*text, "project \"a * / b c\""));
  EXPECT_EQ(text->find("*/"), text->find(" */\n\n#ifndef") + 1);
}

TEST(CHeaderTest, PhromHeaderRefusesNamesThatDoNotMatchTheWords) {
  std::vector<CHeaderName> names = Names();
  names.pop_back();
  EXPECT_FALSE(CHeader::ForPhroms(Info(), Images(), names).IsOk());
  names = Names();
  names[1].name = "OTHER";
  EXPECT_FALSE(CHeader::ForPhroms(Info(), Images(), names).IsOk());
  names = Names();
  names.push_back({.name = "MORE", .identifier = "MORE"});
  EXPECT_FALSE(CHeader::ForPhroms(Info(), Images(), names).IsOk());
}

TEST(CHeaderTest, StreamsHaveTheirBytesLengthsAndTable) {
  const std::vector<CHeaderStream> streams = {
      {.name = {.name = "HELLO", .identifier = "HELLO"},
       .bytes = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09}},
      {.name = {.name = "O'CLOCK", .identifier = "O_CLOCK"},
       .bytes = std::vector<std::uint8_t>(30, 0xA5)}};
  const StatusOr<std::string> text = CHeader::ForStreams(Info(), streams);
  ASSERT_TRUE(text.IsOk()) << text.GetStatus().ToString();
  EXPECT_TRUE(Contains(*text, "#define SPEECH_WORDS 2\n"));
  EXPECT_TRUE(Contains(*text, "#define SPEECH_LENGTH_HELLO 9\n"));
  EXPECT_TRUE(Contains(*text, "#define SPEECH_LENGTH_O_CLOCK 30\n"));
  EXPECT_EQ(ArrayBytes(*text, "speech_word_hello"), streams[0].bytes);
  EXPECT_EQ(ArrayBytes(*text, "speech_word_o_clock"), streams[1].bytes);
  EXPECT_TRUE(Contains(
      *text, "  {\"O'CLOCK\", speech_word_o_clock, SPEECH_LENGTH_O_CLOCK},\n"));
  EXPECT_TRUE(Contains(*text, "Speak External"));
}

TEST(CHeaderTest, RefusesAStreamTooLongForItsLength) {
  const std::vector<CHeaderStream> streams = {
      {.name = {.name = "LONG", .identifier = "LONG"},
       .bytes = std::vector<std::uint8_t>(kMaxCHeaderStreamBytes + 1, 0)}};
  EXPECT_EQ(CHeader::ForStreams(Info(), streams).GetStatus().Code(),
            StatusCode::kResourceExhausted);
}

TEST(CHeaderTest, ChecksThePrefixAndLicence) {
  EXPECT_TRUE(CHeader::CheckInfo(Info()).IsOk());
  for (const char* prefix : {"", "2x", "a-b", "a b"}) {
    CHeaderInfo info = Info();
    info.prefix = prefix;
    EXPECT_FALSE(CHeader::CheckInfo(info).IsOk()) << prefix;
  }
  for (const char* licence : {"", "  ", "MIT */", "GPL;", "A\nB"}) {
    CHeaderInfo info = Info();
    info.licence = licence;
    EXPECT_FALSE(CHeader::CheckInfo(info).IsOk()) << licence;
  }
  CHeaderInfo info = Info();
  info.licence = "(MIT OR Apache-2.0) AND GPL-2.0+";
  EXPECT_TRUE(CHeader::CheckInfo(info).IsOk());
}

}  // namespace
}  // namespace speak5220::core::packaging
