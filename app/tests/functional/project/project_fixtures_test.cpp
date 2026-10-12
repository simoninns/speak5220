/*
 * File:        project_fixtures_test.cpp
 * Module:      project tests
 * Purpose:     Loads the fixture projects in app/projects/ and checks what
 *              each one is for; builds the 64-PHROM budget project, which
 *              is too large to commit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QTemporaryDir>
#include <array>
#include <filesystem>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/bbc/bbc_disc.h"
#include "core/lpc/frame.h"
#include "core/packaging/phrom_image.h"
#include "core/packaging/speak_external_stream.h"
#include "core/project/directory_project_store.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/project/project_serializer.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::core::project {
namespace {

namespace fs = std::filesystem;

constexpr std::array<const char*, 8> kFixtures = {
    "general/one-phrase", "general/many-phrases",      "general/frame-kinds",
    "phrom/one-phrom",    "phrom/two-phroms",          "bbc/one-phrom",
    "bbc/both-sides",     "benchmark/synthetic-speech"};

fs::path FixturePath(const std::string& name) {
  return fs::path(SPEAK5220_PROJECTS_DIR) / name;
}

class ProjectFixturesTest : public ::testing::Test {
 protected:
  // Loads a fixture; the test fails, naming the path, if it cannot.
  Project LoadFixture(const std::string& name) {
    const fs::path path = FixturePath(name);
    EXPECT_TRUE(fs::is_directory(path)) << "missing fixture " << path;
    DirectoryProjectStore store(path);
    StatusOr<Project> project = serializer_.Load(store);
    EXPECT_TRUE(project.IsOk())
        << path << ": " << project.GetStatus().ToString();
    return project.IsOk() ? *std::move(project) : Project();
  }

  static std::set<std::string> Names(const Project& project) {
    return project.PhraseNames();
  }

  audio::SndfileWavCodec codec_;
  synth::Synth synth_;
  ProjectSerializer serializer_{codec_};
};

TEST_F(ProjectFixturesTest, EveryFixtureSavesBackToItsOwnFiles) {
  for (const char* name : kFixtures) {
    DirectoryProjectStore store(FixturePath(name));
    const StatusOr<ProjectFiles> files = store.Read();
    ASSERT_TRUE(files.IsOk()) << name << ": " << files.GetStatus().ToString();
    const StatusOr<Project> project = serializer_.FromFiles(*files);
    ASSERT_TRUE(project.IsOk())
        << name << ": " << project.GetStatus().ToString();
    const StatusOr<ProjectFiles> saved = serializer_.ToFiles(*project);
    ASSERT_TRUE(saved.IsOk());
    EXPECT_EQ(*saved, *files) << name;
  }
}

TEST_F(ProjectFixturesTest, EveryEncodingIsCurrentAndEveryPresetRuns) {
  for (const char* name : kFixtures) {
    const Project project = LoadFixture(name);
    for (const Phrase& phrase : project.Phrases()) {
      const StatusOr<bool> current =
          EncodingCache::IsCurrent(phrase, project.Settings().default_encoder);
      ASSERT_TRUE(current.IsOk()) << name;
      EXPECT_TRUE(*current) << name << ": " << phrase.name;
    }
    ASSERT_FALSE(project.Contents().exports.empty()) << name;
    for (const ExportPreset& preset : project.Contents().exports) {
      const StatusOr<std::vector<ExportedFile>> files =
          RunExport(project, preset, preset.name,
                    {.synth = &synth_, .wav_codec = &codec_});
      ASSERT_TRUE(files.IsOk()) << name << ": " << files.GetStatus().ToString();
      EXPECT_FALSE(files->empty()) << name;
    }
  }
}

TEST_F(ProjectFixturesTest, OnePhraseHoldsOneEncodedPhrase) {
  const Project project = LoadFixture("general/one-phrase");
  ASSERT_EQ(project.Phrases().size(), 1U);
  const Phrase& phrase = project.Phrases()[0];
  EXPECT_EQ(phrase.name, "HELLO");
  EXPECT_FALSE(phrase.locked);
  EXPECT_FALSE(phrase.audio.empty());
  ASSERT_TRUE(phrase.encoding.has_value());
  EXPECT_EQ(phrase.encoding->frames.back(), lpc::Frame::Stop());
}

TEST_F(ProjectFixturesTest, ManyPhrasesHasAcornNamesAndAnAsciiMap) {
  const Project project = LoadFixture("general/many-phrases");
  EXPECT_EQ(Names(project),
            (std::set<std::string>{"ONE,WON", "2- (TWEN-)", "O'CLOCK", "-ING",
                                   "(TONE 1)", "SEVEN"}));
  const ProjectSettings& settings = project.Settings();
  EXPECT_EQ(settings.ascii_map.at(' '), "(TONE 1)");
  EXPECT_EQ(settings.ascii_map.at('1'), "ONE,WON");
  EXPECT_EQ(settings.ascii_map.size(), 4U);
  const Phrase* seven = nullptr;
  for (const Phrase& phrase : project.Phrases()) {
    if (phrase.name == "SEVEN") {
      seven = &phrase;
    }
  }
  ASSERT_NE(seven, nullptr);
  EXPECT_EQ(seven->encoder.at("gate_db"), "40");
  // A preset per format and PHROM layout, and both scopes and WAV forms;
  // the BBC Micro disc has fixtures of its own (bbc/).
  std::set<std::string> formats;
  for (const ExportPreset& preset : project.Contents().exports) {
    formats.insert(preset.format);
  }
  EXPECT_FALSE(formats.contains(std::string(kBbcDiscFormat)));
  EXPECT_EQ(formats.size(), ExportFormats().size() - 1);
  EXPECT_EQ(project.Contents().exports.size(), 6U);
}

TEST_F(ProjectFixturesTest, FrameKindsHasEveryKindOfFrame) {
  const Project project = LoadFixture("general/frame-kinds");
  ASSERT_EQ(project.Phrases().size(), 1U);
  const Phrase& phrase = project.Phrases()[0];
  EXPECT_TRUE(phrase.locked);
  EXPECT_FALSE(phrase.source.has_value());
  std::set<lpc::FrameType> kinds;
  for (const lpc::Frame& frame : phrase.encoding->frames) {
    kinds.insert(lpc::FrameKind(frame));
  }
  EXPECT_EQ(kinds, (std::set<lpc::FrameType>{
                       lpc::FrameType::kSilence, lpc::FrameType::kStop,
                       lpc::FrameType::kRepeat, lpc::FrameType::kUnvoiced,
                       lpc::FrameType::kVoiced}));
}

TEST_F(ProjectFixturesTest, OnePhromFillsExactlyOnePhrom) {
  const Project project = LoadFixture("phrom/one-phrom");
  const PhromAllocation allocation = project.Allocation();
  EXPECT_EQ(allocation.PhromsUsed(), 1);
  EXPECT_EQ(allocation.FreeInLastPhrom(), 0);
  const StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(project);
  ASSERT_TRUE(images.IsOk()) << images.GetStatus().ToString();
  ASSERT_EQ(images->size(), 1U);
  EXPECT_EQ(images->front().table.size(), 4U);
}

TEST_F(ProjectFixturesTest, TwoPhromsSpillsIntoASecond) {
  const Project project = LoadFixture("phrom/two-phroms");
  const PhromAllocation allocation = project.Allocation();
  ASSERT_EQ(allocation.PhromsUsed(), 2);
  ASSERT_EQ(project.Phrases().size(), 3U);
  EXPECT_EQ(project.Phrases()[2].name, "THIRD");
  EXPECT_EQ(allocation.phrases[2].phrom, 2);
  EXPECT_EQ(allocation.phrases[2].word_number, 127);
  const StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(project);
  ASSERT_TRUE(images.IsOk());
  EXPECT_EQ(images->size(), 2U);
}

TEST_F(ProjectFixturesTest, BbcOnePhromHasTwoPagesOfPhrases) {
  const Project project = LoadFixture("bbc/one-phrom");
  EXPECT_EQ(project.Allocation().PhromsUsed(), 1);
  // The player lists 16 phrases a page.
  EXPECT_EQ(project.Phrases().size(), 27U);
  // SHORT's stream is padded to the 9 bytes speech needs to start.
  const Phrase* shortest = nullptr;
  for (const Phrase& phrase : project.Phrases()) {
    if (phrase.name == "SHORT") {
      shortest = &phrase;
    }
  }
  ASSERT_NE(shortest, nullptr);
  EXPECT_EQ(
      packaging::SpeakExternalStream::Build(shortest->encoding->frames)->size(),
      packaging::SpeakExternalStream::kMinimumBytes);
}

TEST_F(ProjectFixturesTest, BbcBothSidesFillsFivePhroms) {
  const Project project = LoadFixture("bbc/both-sides");
  const PhromAllocation allocation = project.Allocation();
  ASSERT_EQ(allocation.PhromsUsed(), 5);
  // PHROM 1 holds the largest phrase a PHROM can.
  EXPECT_EQ(project.Phrases()[0].name, "LONGEST");
  EXPECT_EQ(allocation.phrases[0].cost, kPhromWordCapacity);
  std::vector<const Phrase*> phrases;
  for (const Phrase& phrase : project.Phrases()) {
    phrases.push_back(&phrase);
  }
  const StatusOr<std::vector<bbc::BbcDiscPhrom>> phroms =
      BuildBbcDiscPhroms(project.Settings(), phrases);
  ASSERT_TRUE(phroms.IsOk()) << phroms.GetStatus().ToString();
  EXPECT_EQ((*phroms)[2].title, "The third PHROM");
  // PHROM 5, the first on side 1, holds three phrases.
  EXPECT_EQ((*phroms)[4].phrases.size(), 3U);
}

// The 64-PHROM budget project is made here rather than committed: its
// frames would take several megabytes of YAML (TESTING.md, mechanically
// derived data).
TEST_F(ProjectFixturesTest, ProjectAtTheBudgetSavesLoadsAndRefusesMore) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  std::vector<Phrase> phrases;
  phrases.reserve(kPhromBudget);
  for (int i = 0; i < kPhromBudget; ++i) {
    phrases.push_back(testing::LockedPhrase(
        i + 1, "PHROM " + std::to_string(i + 1), kPhromWordCapacity));
  }
  const Project made = testing::ProjectWith(std::move(phrases));
  const fs::path path = fs::path(directory.path().toStdString()) / "budget";
  DirectoryProjectStore store(path);
  ASSERT_TRUE(serializer_.Save(made, store).IsOk());
  StatusOr<Project> loaded = serializer_.Load(store);
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(*loaded, made);
  EXPECT_EQ(loaded->Allocation().PhromsUsed(), kPhromBudget);
  const StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(*loaded);
  ASSERT_TRUE(images.IsOk());
  EXPECT_EQ(images->size(), static_cast<std::size_t>(kPhromBudget));
  EXPECT_EQ(
      loaded
          ->InsertPhrases({{.index = kPhromBudget,
                            .phrase = testing::LockedPhrase(100, "MORE", 10)}})
          .Code(),
      StatusCode::kResourceExhausted);
}

}  // namespace
}  // namespace speak5220::core::project
