/*
 * File:        project_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the project's editing operations, each with
 *              its reverse, and of its rules
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project.h"

#include <string>
#include <utility>
#include <vector>

#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::core::project {
namespace {

using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::LockedPhrase;
using testing::ProjectWith;
using testing::TestId;

// Five phrases, ids 1 to 5, named so that sorting changes their order.
Project FivePhrases() {
  return ProjectWith(
      {EncodedPhrase(1, "ECHO", 100), EncodedPhrase(2, "ALPHA", 100),
       EncodedPhrase(3, "DELTA", 100), EncodedPhrase(4, "BRAVO", 100),
       EncodedPhrase(5, "CHARLIE", 100)});
}

std::vector<PhraseId> Ids(const std::vector<int>& numbers) {
  std::vector<PhraseId> ids;
  ids.reserve(numbers.size());
  for (const int n : numbers) {
    ids.push_back(TestId(n));
  }
  return ids;
}

// A project whose PHROMs 1 to 63 are full, and whose 64th holds `last`
// bytes of words.
Project ProjectNearTheBudget(int last) {
  std::vector<Phrase> phrases;
  phrases.reserve(kPhromBudget);
  for (int i = 0; i < kPhromBudget - 1; ++i) {
    phrases.push_back(
        LockedPhrase(i + 1, "P" + std::to_string(i), kPhromWordCapacity));
  }
  phrases.push_back(LockedPhrase(kPhromBudget, "LAST", last));
  return ProjectWith(std::move(phrases));
}

TEST(ProjectTest, InsertPutsPhrasesAtTheirIndices) {
  Project project = FivePhrases();
  ASSERT_TRUE(project
                  .InsertPhrases({{.index = 0, .phrase = AudioPhrase(6, "F")},
                                  {.index = 3, .phrase = AudioPhrase(7, "G")},
                                  {.index = 7, .phrase = AudioPhrase(8, "H")}})
                  .IsOk());
  EXPECT_EQ(project.Order(), Ids({6, 1, 2, 7, 3, 4, 5, 8}));
}

TEST(ProjectTest, InsertIsReversedByRemove) {
  Project project = FivePhrases();
  const Project before = project;
  ASSERT_TRUE(project
                  .InsertPhrases({{.index = 1, .phrase = AudioPhrase(6, "F")},
                                  {.index = 4, .phrase = AudioPhrase(7, "G")}})
                  .IsOk());
  ASSERT_TRUE(project.RemovePhrases(Ids({6, 7})).IsOk());
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, InsertNormalisesNames) {
  Project project;
  ASSERT_TRUE(
      project.InsertPhrases({{.index = 0, .phrase = AudioPhrase(1, "hello ")}})
          .IsOk());
  EXPECT_EQ(project.Phrases()[0].name, "HELLO");
}

TEST(ProjectTest, InsertRefusesAndChangesNothingOnAnyBadPhrase) {
  Project project = FivePhrases();
  const Project before = project;
  Phrase no_source = AudioPhrase(9, "NOSOURCE");
  no_source.source.reset();
  no_source.audio.clear();
  Phrase bad_id = AudioPhrase(9, "BADID");
  bad_id.id = "../escape";
  Phrase bad_override = AudioPhrase(9, "OVERRIDE");
  bad_override.encoder["target_rms"] = "1000";
  Phrase bad_frame = EncodedPhrase(9, "FRAME", 10);
  bad_frame.encoding->frames[0].energy = 16;
  const std::vector<Phrase> bad = {
      AudioPhrase(1, "NEW"),    // id taken
      AudioPhrase(9, "ALPHA"),  // name taken
      AudioPhrase(9, "BAD!"),  no_source, bad_id, bad_override, bad_frame};
  for (const Phrase& phrase : bad) {
    EXPECT_FALSE(
        project
            .InsertPhrases({{.index = 0, .phrase = AudioPhrase(8, "OK")},
                            {.index = 1, .phrase = phrase}})
            .IsOk())
        << phrase.name;
    EXPECT_EQ(project, before) << phrase.name;
  }
}

TEST(ProjectTest, InsertRefusesIndicesOutOfOrderOrRange) {
  Project project = FivePhrases();
  EXPECT_FALSE(
      project.InsertPhrases({{.index = 6, .phrase = AudioPhrase(6, "F")}})
          .IsOk());
  EXPECT_FALSE(project
                   .InsertPhrases({{.index = 2, .phrase = AudioPhrase(6, "F")},
                                   {.index = 2, .phrase = AudioPhrase(7, "G")}})
                   .IsOk());
  EXPECT_FALSE(
      project.InsertPhrases({{.index = -1, .phrase = AudioPhrase(6, "F")}})
          .IsOk());
}

TEST(ProjectTest, InsertRefusesTwoNewPhrasesWithOneName) {
  Project project;
  EXPECT_FALSE(project
                   .InsertPhrases({{.index = 0, .phrase = AudioPhrase(1, "A")},
                                   {.index = 1, .phrase = AudioPhrase(2, "a")}})
                   .IsOk());
  EXPECT_TRUE(project.Phrases().empty());
}

TEST(ProjectTest, InsertThatWouldNeedA65thPhromIsRefused) {
  Project project = ProjectNearTheBudget(16000);
  const Project before = project;
  const Status status = project.InsertPhrases(
      {{.index = 64, .phrase = EncodedPhrase(100, "MORE", 200)}});
  EXPECT_EQ(status.Code(), StatusCode::kResourceExhausted);
  EXPECT_NE(status.Message().find("65 PHROMs"), std::string::npos);
  EXPECT_EQ(project, before);
  // A smaller phrase still fits in PHROM 64.
  EXPECT_TRUE(project
                  .InsertPhrases({{.index = 64,
                                   .phrase = EncodedPhrase(100, "MORE", 128)}})
                  .IsOk());
  EXPECT_EQ(project.Allocation().PhromsUsed(), 64);
}

TEST(ProjectTest, UnencodedPhrasesNeverExceedTheBudget) {
  Project project = ProjectNearTheBudget(kPhromWordCapacity);
  EXPECT_TRUE(
      project.InsertPhrases({{.index = 0, .phrase = AudioPhrase(100, "NEW")}})
          .IsOk());
}

TEST(ProjectTest, RemoveReturnsPhrasesWithTheirIndices) {
  Project project = FivePhrases();
  const Project before = project;
  StatusOr<std::vector<PlacedPhrase>> removed =
      project.RemovePhrases(Ids({4, 2}));
  ASSERT_TRUE(removed.IsOk());
  ASSERT_EQ(removed->size(), 2U);
  EXPECT_EQ((*removed)[0].index, 1);
  EXPECT_EQ((*removed)[0].phrase, before.Phrases()[1]);
  EXPECT_EQ((*removed)[1].index, 3);
  EXPECT_EQ(project.Order(), Ids({1, 3, 5}));
}

TEST(ProjectTest, RemoveIsReversedByInsert) {
  Project project = FivePhrases();
  const Project before = project;
  StatusOr<std::vector<PlacedPhrase>> removed =
      project.RemovePhrases(Ids({5, 1, 3}));
  ASSERT_TRUE(removed.IsOk());
  ASSERT_TRUE(project.InsertPhrases(*std::move(removed)).IsOk());
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, RemoveRefusesUnknownOrRepeatedIds) {
  Project project = FivePhrases();
  EXPECT_EQ(project.RemovePhrases(Ids({9})).GetStatus().Code(),
            StatusCode::kNotFound);
  EXPECT_FALSE(project.RemovePhrases(Ids({2, 2})).IsOk());
  EXPECT_EQ(project.Phrases().size(), 5U);
}

TEST(ProjectTest, MovePlacesPhrasesBeforeTheDestination) {
  Project project = FivePhrases();
  ASSERT_TRUE(project.MovePhrases(Ids({1}), 3).IsOk());
  EXPECT_EQ(project.Order(), Ids({2, 3, 1, 4, 5}));
}

TEST(ProjectTest, MoveKeepsTheRelativeOrderOfTheMovedPhrases) {
  Project project = FivePhrases();
  ASSERT_TRUE(project.MovePhrases(Ids({5, 2}), 0).IsOk());
  EXPECT_EQ(project.Order(), Ids({2, 5, 1, 3, 4}));
  ASSERT_TRUE(project.MovePhrases(Ids({2, 5}), 5).IsOk());
  EXPECT_EQ(project.Order(), Ids({1, 3, 4, 2, 5}));
}

TEST(ProjectTest, MoveIsReversedBySetOrder) {
  Project project = FivePhrases();
  const Project before = project;
  StatusOr<std::vector<PhraseId>> previous =
      project.MovePhrases(Ids({1, 4}), 3);
  ASSERT_TRUE(previous.IsOk());
  EXPECT_EQ(*previous, Ids({1, 2, 3, 4, 5}));
  ASSERT_TRUE(project.SetOrder(*previous).IsOk());
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, MoveRefusesABadDestinationOrId) {
  Project project = FivePhrases();
  EXPECT_FALSE(project.MovePhrases(Ids({1}), 6).IsOk());
  EXPECT_FALSE(project.MovePhrases(Ids({1}), -1).IsOk());
  EXPECT_FALSE(project.MovePhrases(Ids({9}), 0).IsOk());
  EXPECT_FALSE(project.MovePhrases(Ids({1, 1}), 0).IsOk());
  EXPECT_EQ(project.Order(), Ids({1, 2, 3, 4, 5}));
}

TEST(ProjectTest, SortOrdersByNameAndIsReversedBySetOrder) {
  Project project = FivePhrases();
  const Project before = project;
  StatusOr<std::vector<PhraseId>> previous = project.SortPhrases();
  ASSERT_TRUE(previous.IsOk());
  EXPECT_EQ(project.Order(), Ids({2, 4, 5, 3, 1}));
  ASSERT_TRUE(project.SetOrder(*previous).IsOk());
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, SetOrderNeedsEveryPhraseOnce) {
  Project project = FivePhrases();
  EXPECT_FALSE(project.SetOrder(Ids({1, 2, 3, 4})).IsOk());
  EXPECT_FALSE(project.SetOrder(Ids({1, 2, 3, 4, 4})).IsOk());
  EXPECT_FALSE(project.SetOrder(Ids({1, 2, 3, 4, 9})).IsOk());
  EXPECT_EQ(project.Order(), Ids({1, 2, 3, 4, 5}));
}

TEST(ProjectTest, ReorderingThatWouldNeedA65thPhromIsRefused) {
  // PHROMs 63 and 64 each hold a big and a small phrase; moving the first
  // small one to the end leaves it no room in PHROM 64.
  std::vector<Phrase> phrases;
  phrases.reserve(kPhromBudget + 2);
  for (int i = 0; i < kPhromBudget - 2; ++i) {
    phrases.push_back(
        LockedPhrase(i + 1, "P" + std::to_string(i), kPhromWordCapacity));
  }
  phrases.push_back(LockedPhrase(101, "BIG1", 10000));
  phrases.push_back(LockedPhrase(102, "SMALL1", 6000));
  phrases.push_back(LockedPhrase(103, "BIG2", 10000));
  phrases.push_back(LockedPhrase(104, "SMALL2", 6000));
  Project project = ProjectWith(std::move(phrases));
  ASSERT_EQ(project.Allocation().PhromsUsed(), 64);
  const Project before = project;
  EXPECT_EQ(project.MovePhrases(Ids({102}), 66).GetStatus().Code(),
            StatusCode::kResourceExhausted);
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, RenameIsReversedByRename) {
  Project project = FivePhrases();
  const Project before = project;
  StatusOr<std::string> previous = project.RenamePhrase(TestId(2), "zulu");
  ASSERT_TRUE(previous.IsOk());
  EXPECT_EQ(*previous, "ALPHA");
  EXPECT_EQ(project.FindPhrase(TestId(2))->name, "ZULU");
  ASSERT_TRUE(project.RenamePhrase(TestId(2), *previous).IsOk());
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, RenameRefusesTakenOrInvalidNames) {
  Project project = FivePhrases();
  EXPECT_FALSE(project.RenamePhrase(TestId(2), "bravo").IsOk());
  EXPECT_FALSE(project.RenamePhrase(TestId(2), "A&B").IsOk());
  EXPECT_FALSE(project.RenamePhrase(TestId(9), "NEW").IsOk());
  EXPECT_EQ(project.FindPhrase(TestId(2))->name, "ALPHA");
  // Its own name, in any case, is no change.
  EXPECT_TRUE(project.RenamePhrase(TestId(2), "alpha").IsOk());
}

TEST(ProjectTest, RenameThatWouldNeedA65thPhromIsRefused) {
  // PHROM 64 is exactly full with LAST and TAIL; a longer name for LAST
  // costs more bytes, which pushes TAIL into a 65th PHROM.
  Project project = ProjectNearTheBudget(8000);
  ASSERT_TRUE(project
                  .InsertPhrases({{.index = 64,
                                   .phrase = LockedPhrase(200, "TAIL", 8128)}})
                  .IsOk());
  ASSERT_EQ(project.Allocation().FreeInLastPhrom(), 0);
  EXPECT_EQ(
      project.RenamePhrase(TestId(kPhromBudget), "LONGER").GetStatus().Code(),
      StatusCode::kResourceExhausted);
  EXPECT_EQ(project.FindPhrase(TestId(kPhromBudget))->name, "LAST");
  EXPECT_TRUE(project.RenamePhrase(TestId(kPhromBudget), "TSAL").IsOk());
}

TEST(ProjectTest, DescriptionIsReversedBySetDescription) {
  Project project = FivePhrases();
  const Project before = project;
  StatusOr<std::string> previous = project.SetDescription(
      TestId(3), "Greeting, recorded 2026-10-10 \xc2\xb7");
  ASSERT_TRUE(previous.IsOk());
  EXPECT_EQ(*previous, "");
  ASSERT_TRUE(project.SetDescription(TestId(3), *previous).IsOk());
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, DescriptionMustBeUtf8) {
  Project project = FivePhrases();
  EXPECT_FALSE(project.SetDescription(TestId(3), "bad \xff byte").IsOk());
  EXPECT_FALSE(project.SetDescription(TestId(3), "overlong \xc0\xaf").IsOk());
  EXPECT_FALSE(project.SetDescription(TestId(3), "cut \xe2\x82").IsOk());
  EXPECT_TRUE(project.SetDescription(TestId(3), "\xf0\x9f\x94\x8a").IsOk());
}

TEST(ProjectTest, SetEncodingIsReversedBySetEncoding) {
  Project project = FivePhrases();
  const Project before = project;
  CachedEncoding encoding = *project.FindPhrase(TestId(1))->encoding;
  encoding.frames = testing::FramesPackingTo(50);
  StatusOr<EncodingChange> previous = project.SetEncoding(
      TestId(1), {.overrides = {{"target_rms", "18"}}, .encoding = encoding});
  ASSERT_TRUE(previous.IsOk());
  EXPECT_TRUE(previous->overrides.empty());
  EXPECT_EQ(project.FindPhrase(TestId(1))->encoder.at("target_rms"), "18");
  ASSERT_TRUE(project.SetEncoding(TestId(1), *std::move(previous)).IsOk());
  EXPECT_EQ(project, before);
}

TEST(ProjectTest, SetEncodingRefusesLockedPhrasesAndBadOverrides) {
  Project project = ProjectWith(
      {LockedPhrase(1, "LOCKED", 10), EncodedPhrase(2, "OPEN", 10)});
  EXPECT_EQ(project.SetEncoding(TestId(1), {}).GetStatus().Code(),
            StatusCode::kFailedPrecondition);
  EXPECT_FALSE(
      project.SetEncoding(TestId(2), {.overrides = {{"bogus", "1"}}}).IsOk());
  EXPECT_FALSE(
      project.SetEncoding(TestId(2), {.overrides = {{"window", "1.5"}}})
          .IsOk());
}

TEST(ProjectTest, SetEncodingThatWouldNeedA65thPhromKeepsThePrevious) {
  // "LAST" is locked, so add an unlocked phrase to re-encode.
  Project open = ProjectNearTheBudget(15000);
  ASSERT_TRUE(open.InsertPhrases({{.index = 64,
                                   .phrase = EncodedPhrase(200, "OPEN", 500)}})
                  .IsOk());
  const Project open_before = open;
  CachedEncoding bigger = *open.FindPhrase(TestId(200))->encoding;
  bigger.frames = testing::FramesPackingTo(2000);
  const StatusOr<EncodingChange> result = open.SetEncoding(
      TestId(200), {.overrides = {{"repeats", "false"}}, .encoding = bigger});
  EXPECT_EQ(result.GetStatus().Code(), StatusCode::kResourceExhausted);
  EXPECT_EQ(open, open_before);
}

TEST(ProjectTest, OverBudgetProjectMayStillShrink) {
  std::vector<Phrase> phrases;
  phrases.reserve(kPhromBudget + 1);
  for (int i = 0; i < kPhromBudget + 1; ++i) {
    phrases.push_back(
        LockedPhrase(i + 1, "P" + std::to_string(i), kPhromWordCapacity));
  }
  Project project = ProjectWith(std::move(phrases));
  ASSERT_EQ(project.Allocation().PhromsUsed(), 65);
  EXPECT_TRUE(project.RenamePhrase(TestId(1), "Q0").IsOk());
  EXPECT_TRUE(project.RemovePhrases(Ids({1})).IsOk());
  EXPECT_EQ(project.Allocation().PhromsUsed(), 64);
}

TEST(ProjectTest, FromContentsChecksTheRules) {
  ProjectContents duplicate_names;
  duplicate_names.phrases = {AudioPhrase(1, "A"), AudioPhrase(2, "A")};
  EXPECT_FALSE(Project::FromContents(duplicate_names).IsOk());
  ProjectContents duplicate_ids;
  duplicate_ids.phrases = {AudioPhrase(1, "A"), AudioPhrase(1, "B")};
  EXPECT_FALSE(Project::FromContents(duplicate_ids).IsOk());
  ProjectContents bad_map;
  bad_map.settings.ascii_map['\n'] = "A";
  EXPECT_FALSE(Project::FromContents(bad_map).IsOk());
  ProjectContents bad_phrom;
  bad_phrom.settings.phroms = {
      {.number = 65, .header_strings = {}, .unknown = {}}};
  EXPECT_FALSE(Project::FromContents(bad_phrom).IsOk());
  ProjectContents bad_exports;
  bad_exports.exports = {{.name = "x"}, {.name = "x"}};
  EXPECT_FALSE(Project::FromContents(bad_exports).IsOk());
}

TEST(ProjectTest, SettingsMustKeepEveryOverrideValid) {
  Project project = ProjectWith({AudioPhrase(1, "A")});
  ASSERT_TRUE(
      project.SetEncoding(TestId(1), {.overrides = {{"target_rms", "18"}}})
          .IsOk());
  ProjectSettings settings = project.Settings();
  settings.name = "Renamed";
  settings.ascii_map['A'] = "A";
  EXPECT_TRUE(project.SetSettings(settings).IsOk());
  settings.default_encoder.target_rms = 0.0;
  EXPECT_FALSE(project.SetSettings(settings).IsOk());
  EXPECT_EQ(project.Settings().name, "Renamed");
}

TEST(ProjectTest, AllocationCostsNamesAndPointers) {
  Project project = ProjectWith(
      {EncodedPhrase(1, "HELLO", 107), LockedPhrase(2, "WORLD", 50)});
  const PhromAllocation allocation = project.Allocation();
  EXPECT_EQ(allocation.phrases[0].cost, 107);
  EXPECT_EQ(allocation.phrases[1].cost, 50);
  EXPECT_EQ(allocation.phroms[0].used_bytes, 256 + 157);
}

}  // namespace
}  // namespace speak5220::core::project
