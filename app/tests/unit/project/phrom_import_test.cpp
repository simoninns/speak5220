/*
 * File:        phrom_import_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of importing Acorn PHROM images as locked
 *              phrases: order, names, encodings and failures
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrom_import.h"

#include <cstddef>
#include <cstdint>
#include <map>
#include <random>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "core/packaging/acorn_phrom.h"
#include "core/packaging/phrom_image.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_editor.h"
#include "core/project/project_export.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::project {
namespace {

using lpc::Frame;
using packaging::AcornPhrom;
using packaging::Word;
using speak5220::testing::SpeechLikeFrames;
using ::testing::ElementsAre;
using ::testing::HasSubstr;

// The phrases ImportPhromImage() makes of `bytes`.
StatusOr<std::vector<Phrase>> ImportWords(std::span<const std::uint8_t> bytes,
                                          const std::set<std::string>& taken) {
  StatusOr<PhromImport> imported = ImportPhromImage(bytes, taken);
  if (!imported.IsOk()) {
    return imported.GetStatus();
  }
  return std::move(imported->phrases);
}

std::vector<std::uint8_t> Image(const std::vector<Word>& words) {
  StatusOr<packaging::PhromImage> built = AcornPhrom::Build(words, {});
  EXPECT_TRUE(built.IsOk()) << built.GetStatus().ToString();
  return built.IsOk() ? std::move(built->bytes) : std::vector<std::uint8_t>{};
}

std::vector<std::string> Names(const std::vector<Phrase>& phrases) {
  std::vector<std::string> names;
  names.reserve(phrases.size());
  for (const Phrase& phrase : phrases) {
    names.push_back(phrase.name);
  }
  return names;
}

class PhromImportTest : public ::testing::Test {
 protected:
  void SetUp() override {
    std::mt19937 rng(21);
    first_ = {{"ONE", SpeechLikeFrames(rng, 10)},
              {"TWO", SpeechLikeFrames(rng, 12)}};
    second_ = {{"THREE", SpeechLikeFrames(rng, 8)}};
  }

  std::vector<Word> first_;
  std::vector<Word> second_;
};

TEST_F(PhromImportTest, MakesLockedPhrasesFromFramesAlone) {
  const StatusOr<std::vector<Phrase>> phrases = ImportWords(Image(first_), {});
  ASSERT_TRUE(phrases.IsOk()) << phrases.GetStatus().ToString();
  ASSERT_EQ(phrases->size(), 2U);
  for (std::size_t i = 0; i < phrases->size(); ++i) {
    const Phrase& phrase = (*phrases)[i];
    EXPECT_TRUE(IsValidPhraseId(phrase.id));
    EXPECT_EQ(phrase.name, first_[i].name);
    EXPECT_TRUE(phrase.locked);
    EXPECT_FALSE(phrase.source.has_value());
    EXPECT_TRUE(phrase.audio.empty());
    EXPECT_TRUE(phrase.encoder.empty());
    ASSERT_TRUE(phrase.encoding.has_value());
    EXPECT_EQ(phrase.encoding->frames, first_[i].frames);
    EXPECT_EQ(phrase.encoding->frames.back(), Frame::Stop());
    EXPECT_TRUE(phrase.encoding->encoder_version.empty());
    EXPECT_TRUE(phrase.encoding->settings_hash.empty());
    EXPECT_FALSE(phrase.encoding->quality.has_value());
  }
  EXPECT_NE((*phrases)[0].id, (*phrases)[1].id);
}

TEST_F(PhromImportTest, ReadsConcatenatedImagesInImageThenWordOrder) {
  std::vector<std::uint8_t> bytes = Image(first_);
  const std::vector<std::uint8_t> second = Image(second_);
  bytes.insert(bytes.end(), second.begin(), second.end());
  const StatusOr<std::vector<Phrase>> phrases = ImportWords(bytes, {});
  ASSERT_TRUE(phrases.IsOk()) << phrases.GetStatus().ToString();
  EXPECT_THAT(Names(*phrases), ElementsAre("ONE", "TWO", "THREE"));
  EXPECT_EQ((*phrases)[2].encoding->frames, second_[0].frames);
}

TEST_F(PhromImportTest, MakesNamesValidAndUnique) {
  const std::vector<Frame> stop = {Frame::Stop()};
  const std::vector<Word> words = {
      {"one", stop}, {"TWO", stop}, {"TWO", stop},
      {"A_B", stop}, {"", stop},    {"ABCDEFGHIJKLMNOPQRST", stop},
      {"A  B", stop}};
  const StatusOr<std::vector<Phrase>> phrases =
      ImportWords(Image(words), {"ONE"});
  ASSERT_TRUE(phrases.IsOk()) << phrases.GetStatus().ToString();
  EXPECT_THAT(Names(*phrases),
              ElementsAre("ONE 2", "TWO", "TWO 2", "A B", "WORD 131",
                          "ABCDEFGHIJKLMNOP", "A  B"));
}

TEST_F(PhromImportTest, ImportedPhrasesGoIntoAProject) {
  Project project;
  const StatusOr<std::vector<Phrase>> phrases =
      ImportWords(Image(first_), project.PhraseNames());
  ASSERT_TRUE(phrases.IsOk()) << phrases.GetStatus().ToString();
  std::vector<PlacedPhrase> placed;
  for (const Phrase& phrase : *phrases) {
    placed.push_back(
        {.index = static_cast<int>(placed.size()), .phrase = phrase});
  }
  ASSERT_TRUE(project.InsertPhrases(std::move(placed)).IsOk());
  ASSERT_EQ(project.Phrases().size(), 2U);
  // Locked: their frames are never replaced.
  const StatusOr<EncodingChange> changed =
      project.SetEncoding(project.Phrases()[0].id, {});
  EXPECT_EQ(changed.GetStatus().Code(), StatusCode::kFailedPrecondition);
}

TEST_F(PhromImportTest, ReadsAnImageWithNoWords) {
  const StatusOr<std::vector<Phrase>> phrases = ImportWords(Image({}), {});
  ASSERT_TRUE(phrases.IsOk()) << phrases.GetStatus().ToString();
  EXPECT_TRUE(phrases->empty());
}

TEST_F(PhromImportTest, RejectsEmptyInput) {
  const StatusOr<std::vector<Phrase>> phrases = ImportWords({}, {});
  EXPECT_EQ(phrases.GetStatus().Code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(phrases.GetStatus().Message(), HasSubstr("empty"));
}

TEST_F(PhromImportTest, RejectsASizeThatIsNotWholePhroms) {
  std::vector<std::uint8_t> bytes = Image(first_);
  bytes.push_back(0);
  const StatusOr<std::vector<Phrase>> phrases = ImportWords(bytes, {});
  EXPECT_EQ(phrases.GetStatus().Code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(phrases.GetStatus().Message(), HasSubstr("16385"));
}

TEST_F(PhromImportTest, NamesTheBankThatCannotBeRead) {
  std::vector<std::uint8_t> bytes = Image(first_);
  bytes.resize(2 * packaging::kPhromSize, 0);
  const StatusOr<std::vector<Phrase>> phrases = ImportWords(bytes, {});
  ASSERT_FALSE(phrases.IsOk());
  EXPECT_EQ(phrases.GetStatus().Code(), StatusCode::kInvalidArgument);
  EXPECT_THAT(phrases.GetStatus().Message(), HasSubstr("PHROM 2 of"));
}

TEST_F(PhromImportTest, DoesNotNameTheBankOfASingleImage) {
  const std::vector<std::uint8_t> bytes(packaging::kPhromSize, 0);
  const StatusOr<std::vector<Phrase>> phrases = ImportWords(bytes, {});
  ASSERT_FALSE(phrases.IsOk());
  EXPECT_THAT(phrases.GetStatus().Message(),
              ::testing::Not(HasSubstr("PHROM 1 of")));
}

// One word, "X", that fills a bank to its last byte with every ASCII entry
// naming it, as Acorn's PHROM A fills its bank (see the AcornPhromTest of
// the same layout), with Acorn's header strings.
std::vector<std::uint8_t> FullImage() {
  constexpr int kSilenceFrames = 32251;
  std::vector<Frame> frames(kSilenceFrames, Frame::Silence());
  frames.push_back(Frame::Stop());
  packaging::AcornPhromOptions options;
  options.strings = {"(C)1982 Acorn", "Word PHROM A", "1.00"};
  for (int code = 32; code <= 126; ++code) {
    options.ascii_map[code] = "X";
  }
  const std::vector<Word> words = {{"X", std::move(frames)}};
  StatusOr<packaging::PhromImage> built = AcornPhrom::Build(words, options);
  EXPECT_TRUE(built.IsOk()) << built.GetStatus().ToString();
  return built.IsOk() ? std::move(built->bytes) : std::vector<std::uint8_t>{};
}

TEST_F(PhromImportTest, GivesTheAsciiEntriesAndHeaderStrings) {
  const StatusOr<PhromImport> imported = ImportPhromImage(FullImage(), {"X"});
  ASSERT_TRUE(imported.IsOk()) << imported.GetStatus().ToString();
  ASSERT_EQ(imported->phrases.size(), 1U);
  // The word was renamed, and the entries name the phrase it became.
  const std::string& name = imported->phrases[0].name;
  EXPECT_NE(name, "X");
  EXPECT_EQ(imported->ascii_map.size(), 95U);
  EXPECT_EQ(imported->ascii_map.at('A'), name);
  EXPECT_EQ(imported->header_strings,
            (std::vector<std::vector<std::string>>{
                {"(C)1982 Acorn", "Word PHROM A", "1.00"}}));
}

TEST_F(PhromImportTest, FullImageExportsAgainByteForByte) {
  const std::vector<std::uint8_t> image = FullImage();
  StatusOr<PhromImport> imported = ImportPhromImage(image, {});
  ASSERT_TRUE(imported.IsOk()) << imported.GetStatus().ToString();
  Project project;
  ASSERT_TRUE(project
                  .SetPhromSettings(MergeImportedPhromSettings(
                      project.Settings(), true, *imported))
                  .IsOk());
  std::vector<PlacedPhrase> placed;
  for (Phrase& phrase : imported->phrases) {
    placed.push_back({.index = static_cast<int>(placed.size()),
                      .phrase = std::move(phrase)});
  }
  ASSERT_TRUE(project.InsertPhrases(std::move(placed)).IsOk());
  EXPECT_EQ(project.Allocation().PhromsUsed(), 1);
  const StatusOr<std::vector<packaging::PhromImage>> exported =
      BuildAcornPhroms(project);
  ASSERT_TRUE(exported.IsOk()) << exported.GetStatus().ToString();
  ASSERT_EQ(exported->size(), 1U);
  EXPECT_EQ((*exported)[0].bytes, image);
}

TEST(MergeImportedPhromSettingsTest, AddsOnlyCharactersNotYetMapped) {
  ProjectSettings current;
  current.ascii_map = {{'A', "MINE"}};
  PhromImport imported;
  imported.ascii_map = {{'A', "THEIRS"}, {'B', "BEE"}};
  const PhromSettings merged =
      MergeImportedPhromSettings(current, false, imported);
  EXPECT_EQ(merged.ascii_map,
            (std::map<char, std::string>{{'A', "MINE"}, {'B', "BEE"}}));
}

TEST(MergeImportedPhromSettingsTest, HeaderStringsOnlyForANewDefaultProject) {
  PhromImport imported;
  imported.header_strings = {{"(C)1982 Acorn", "Word PHROM A", "1.00"},
                             {"(C)1983 Acorn", "Word PHROM B", "1.00"}};
  const ProjectSettings defaults;
  const PhromSettings merged =
      MergeImportedPhromSettings(defaults, true, imported);
  ASSERT_EQ(merged.phroms.size(), 2U);
  EXPECT_EQ(merged.phroms[1].number, 2);
  EXPECT_EQ(merged.phroms[1].header_strings, imported.header_strings[1]);

  // Not into a project with phrases, nor over strings of its own.
  EXPECT_TRUE(
      MergeImportedPhromSettings(defaults, false, imported).phroms.empty());
  ProjectSettings own;
  own.header_strings = {"(C)2026 Me", "My PHROM", "2.00"};
  EXPECT_TRUE(MergeImportedPhromSettings(own, true, imported).phroms.empty());
}

}  // namespace
}  // namespace speak5220::core::project
