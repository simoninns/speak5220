/*
 * File:        bbc_disc_export_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the BBC Micro disc export: the PHROMs it
 *              holds, their titles and streams, and the 8-PHROM limit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <string>
#include <utility>
#include <vector>

#include "core/bbc/bbc_disc.h"
#include "core/packaging/speak_external_stream.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::core::project {
namespace {

using testing::EncodedPhrase;
using testing::LockedPhrase;
using testing::ProjectWith;

// `count` phrases, each more than half a PHROM, so one per PHROM.
Project PhromsOfOnePhrase(int count) {
  std::vector<Phrase> phrases;
  for (int i = 1; i <= count; ++i) {
    phrases.push_back(LockedPhrase(i, "P" + std::to_string(i), 9000));
  }
  return ProjectWith(std::move(phrases));
}

std::vector<const Phrase*> All(const Project& project) {
  std::vector<const Phrase*> phrases;
  for (const Phrase& phrase : project.Phrases()) {
    phrases.push_back(&phrase);
  }
  return phrases;
}

TEST(BbcDiscExportTest, PhromsHoldTheirPhrasesStreamsInOrder) {
  // A and B share PHROM 1; C needs PHROM 2.
  const Project project =
      ProjectWith({EncodedPhrase(1, "A", 9000), LockedPhrase(2, "B", 6000),
                   EncodedPhrase(3, "C", 3000)});
  const StatusOr<std::vector<bbc::BbcDiscPhrom>> phroms =
      BuildBbcDiscPhroms(project.Settings(), All(project));
  ASSERT_TRUE(phroms.IsOk()) << phroms.GetStatus().ToString();
  ASSERT_EQ(phroms->size(), 2U);
  EXPECT_EQ((*phroms)[0].title, "Speak5220 PHROM 1");
  EXPECT_EQ((*phroms)[1].title, "Speak5220 PHROM 2");
  ASSERT_EQ((*phroms)[0].phrases.size(), 2U);
  EXPECT_EQ((*phroms)[0].phrases[0].name, "A");
  EXPECT_EQ((*phroms)[0].phrases[1].name, "B");
  EXPECT_EQ((*phroms)[1].phrases[0].name, "C");
  EXPECT_EQ((*phroms)[0].phrases[1].stream,
            packaging::SpeakExternalStream::Build(
                project.Phrases()[1].encoding->frames)
                .Value());
}

TEST(BbcDiscExportTest, PhromTitlesFollowTheirHeaderStrings) {
  Project project = PhromsOfOnePhrase(2);
  ProjectSettings settings = project.Settings();
  settings.header_strings = {"(C)2026 Me", "Numbers", "1.00"};
  settings.phroms = {
      {.number = 2, .header_strings = {"(C)2026 Me", "Letters", "1.00"}}};
  ASSERT_TRUE(project.SetSettings(settings).IsOk());
  const std::vector<bbc::BbcDiscPhrom> phroms =
      BuildBbcDiscPhroms(project.Settings(), All(project)).Value();
  EXPECT_EQ(phroms[0].title, "Numbers 1");
  EXPECT_EQ(phroms[1].title, "Letters");
}

TEST(BbcDiscExportTest, WritesOneDiscImageNamedFromTheBase) {
  const Project project = PhromsOfOnePhrase(5);
  const StatusOr<std::vector<ExportedFile>> files =
      RunExport(project, {.name = "disc", .format = "bbc-dsd"}, "speech");
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  ASSERT_EQ(files->size(), 1U);
  EXPECT_EQ((*files)[0].name, "speech.dsd");
  EXPECT_EQ((*files)[0].bytes,
            bbc::BbcDisc::Build(
                project.Settings().name,
                BuildBbcDiscPhroms(project.Settings(), All(project)).Value())
                .Value());
}

TEST(BbcDiscExportTest, EightPhromsFitAndNineAreRefused) {
  EXPECT_TRUE(RunExport(PhromsOfOnePhrase(bbc::BbcDisc::kMaxPhroms),
                        {.name = "disc", .format = "bbc-dsd"}, "d")
                  .IsOk());
  const Project nine = PhromsOfOnePhrase(bbc::BbcDisc::kMaxPhroms + 1);
  const ExportPreset preset{.name = "disc", .format = "bbc-dsd"};
  const StatusOr<std::vector<ExportedFile>> files =
      RunExport(nine, preset, "d");
  ASSERT_FALSE(files.IsOk());
  EXPECT_EQ(files.GetStatus().Code(), StatusCode::kResourceExhausted);
  EXPECT_NE(files.GetStatus().Message().find("9 PHROMs"), std::string::npos);
  EXPECT_NE(files.GetStatus().Message().find("at most 8"), std::string::npos);
  // The Export dialog says so before the user exports.
  EXPECT_EQ(ExportFileNames(nine, preset, "d").GetStatus(), files.GetStatus());
  // Fewer phrases fit.
  EXPECT_TRUE(RunExport(nine,
                        {.name = "disc",
                         .format = "bbc-dsd",
                         .scope = "selected",
                         .phrases = {"P1", "P9"}},
                        "d")
                  .IsOk());
}

TEST(BbcDiscExportTest, TakesNoOptions) {
  for (const ExportPreset& preset :
       {ExportPreset{.name = "p", .format = "bbc-dsd", .layout = "separate"},
        ExportPreset{.name = "p", .format = "bbc-dsd", .licence = "MIT"},
        ExportPreset{.name = "p", .format = "bbc-dsd", .audio = "original"}}) {
    EXPECT_FALSE(CheckExportPreset(preset).IsOk());
  }
}

}  // namespace
}  // namespace speak5220::core::project
