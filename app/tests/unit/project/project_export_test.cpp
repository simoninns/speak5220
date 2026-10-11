/*
 * File:        project_export_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the PHROM image export: one image per PHROM,
 *              header strings, ASCII entries and file layouts
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_export.h"

#include <string>
#include <vector>

#include "core/lpc/frame_codec.h"
#include "core/packaging/acorn_phrom.h"
#include "core/packaging/phrom_image.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::core::project {
namespace {

using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::LockedPhrase;
using testing::ProjectWith;

// Two PHROMs: A and B fill most of the first, C goes in the second.
Project TwoPhroms() {
  Project project =
      ProjectWith({EncodedPhrase(1, "A", 9000), LockedPhrase(2, "B", 6000),
                   EncodedPhrase(3, "C", 3000)});
  ProjectSettings settings = project.Settings();
  settings.header_strings = {"(C)2026 Test", "My PHROM", "1.00"};
  settings.ascii_map = {{'A', "A"}, {'C', "C"}, {'Z', "GONE"}};
  EXPECT_TRUE(project.SetSettings(settings).IsOk());
  return project;
}

// The text of an image's header strings (bytes 2-55, bit-reversed).
std::vector<std::string> HeaderStrings(const packaging::PhromImage& image) {
  std::vector<std::string> strings(1);
  for (std::size_t i = 2; i < 56; ++i) {
    const auto c = static_cast<char>(lpc::BitReverse(image.bytes[i]));
    if (c == '\0') {
      if (strings.back().empty()) {
        break;
      }
      strings.emplace_back();
    } else {
      strings.back() += c;
    }
  }
  if (strings.back().empty()) {
    strings.pop_back();
  }
  return strings;
}

// The 14-bit pointer of word number `number`.
int Pointer(const packaging::PhromImage& image, int number) {
  const std::size_t at = 2 * static_cast<std::size_t>(number);
  return (image.bytes[at] | (image.bytes[at + 1] << 8)) & 0x3FFF;
}

TEST(ProjectExportTest, AppendsThePhromNumberToTheSecondString) {
  ProjectSettings settings;
  settings.header_strings = {"(C)", "My PHROM", "1.00"};
  EXPECT_EQ(PhromHeaderStrings(settings, 1),
            (std::vector<std::string>{"(C)", "My PHROM 1", "1.00"}));
  EXPECT_EQ(PhromHeaderStrings(settings, 12),
            (std::vector<std::string>{"(C)", "My PHROM 12", "1.00"}));
}

TEST(ProjectExportTest, PhromsOwnHeaderStringsReplaceTheProjects) {
  ProjectSettings settings;
  settings.phroms = {
      {.number = 2, .header_strings = {"(C)", "Numbers"}, .unknown = {}}};
  EXPECT_EQ(PhromHeaderStrings(settings, 2),
            (std::vector<std::string>{"(C)", "Numbers"}));
}

TEST(ProjectExportTest, BuildsOneImagePerPhromInListOrder) {
  const StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(TwoPhroms());
  ASSERT_TRUE(images.IsOk()) << images.GetStatus().ToString();
  ASSERT_EQ(images->size(), 2U);
  ASSERT_EQ((*images)[0].table.size(), 2U);
  EXPECT_EQ((*images)[0].table[0].name, "A");
  EXPECT_EQ((*images)[0].table[0].number, 127);
  EXPECT_EQ((*images)[0].table[1].name, "B");
  ASSERT_EQ((*images)[1].table.size(), 1U);
  EXPECT_EQ((*images)[1].table[0].name, "C");
  EXPECT_EQ((*images)[1].table[0].number, 127);
  EXPECT_EQ(HeaderStrings((*images)[0]),
            (std::vector<std::string>{"(C)2026 Test", "My PHROM 1", "1.00"}));
  EXPECT_EQ(HeaderStrings((*images)[1]),
            (std::vector<std::string>{"(C)2026 Test", "My PHROM 2", "1.00"}));
}

TEST(ProjectExportTest, EachPhromHasOnlyTheAsciiEntriesOfItsWords) {
  const StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(TwoPhroms());
  ASSERT_TRUE(images.IsOk());
  const packaging::PhromImage& first = (*images)[0];
  const packaging::PhromImage& second = (*images)[1];
  EXPECT_EQ(Pointer(first, 'A'), Pointer(first, 127));
  EXPECT_NE(Pointer(first, 'C'), Pointer(first, 127));
  EXPECT_EQ(Pointer(first, 'C'), Pointer(first, ' '));  // the null word
  EXPECT_EQ(Pointer(second, 'C'), Pointer(second, 127));
  EXPECT_EQ(Pointer(second, 'A'), Pointer(second, ' '));
}

TEST(ProjectExportTest, RefusesAPhraseWithNoFrames) {
  const Project project =
      ProjectWith({EncodedPhrase(1, "A", 100), AudioPhrase(2, "NEW")});
  const StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(project);
  ASSERT_FALSE(images.IsOk());
  EXPECT_NE(images.GetStatus().Message().find("NEW"), std::string::npos);
}

TEST(ProjectExportTest, RefusesAPhraseTooLargeForAPhrom) {
  const Project project = ProjectWith({LockedPhrase(1, "HUGE", 17000)});
  EXPECT_EQ(BuildAcornPhroms(project).GetStatus().Code(),
            StatusCode::kResourceExhausted);
}

TEST(ProjectExportTest, SeparateLayoutWritesOneFilePerPhrom) {
  const StatusOr<std::vector<ExportedFile>> files =
      RunExport(TwoPhroms(),
                {.name = "p", .format = "acorn-phrom", .layout = ""}, "words");
  ASSERT_TRUE(files.IsOk());
  ASSERT_EQ(files->size(), 2U);
  EXPECT_EQ((*files)[0].name, "words_1.bin");
  EXPECT_EQ((*files)[1].name, "words_2.bin");
  EXPECT_EQ((*files)[0].bytes.size(), packaging::kPhromSize);
}

TEST(ProjectExportTest, ConcatenatedLayoutWritesOneFile) {
  const Project project = TwoPhroms();
  const StatusOr<std::vector<ExportedFile>> files = RunExport(
      project, {.name = "p", .format = "acorn-phrom", .layout = "concatenated"},
      "words");
  ASSERT_TRUE(files.IsOk());
  ASSERT_EQ(files->size(), 1U);
  EXPECT_EQ((*files)[0].name, "words.bin");
  const StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(project);
  std::vector<std::uint8_t> joined = (*images)[0].bytes;
  joined.insert(joined.end(), (*images)[1].bytes.begin(),
                (*images)[1].bytes.end());
  EXPECT_EQ((*files)[0].bytes, joined);
}

TEST(ProjectExportTest, RefusesWhatIsNotSupportedYet) {
  const Project project = TwoPhroms();
  EXPECT_FALSE(
      RunExport(project, {.name = "p", .format = "c-header"}, "x").IsOk());
  EXPECT_FALSE(
      RunExport(project,
                {.name = "p", .format = "acorn-phrom", .scope = "selected"},
                "x")
          .IsOk());
  EXPECT_FALSE(
      RunExport(project,
                {.name = "p", .format = "acorn-phrom", .layout = "zip"}, "x")
          .IsOk());
}

}  // namespace
}  // namespace speak5220::core::project
