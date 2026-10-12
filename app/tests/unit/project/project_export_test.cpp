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

#include <spdlog/fmt/fmt.h>

#include <set>
#include <string>
#include <string_view>
#include <vector>

#include "core/lpc/frame_codec.h"
#include "core/packaging/acorn_phrom.h"
#include "core/packaging/phrom_image.h"
#include "core/packaging/speak_external_stream.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"
#include "support/synth_mocks.h"

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

TEST(ProjectExportTest, RefusesUnknownFormatsScopesAndLayouts) {
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
                {.name = "p", .format = "acorn-phrom", .scope = "some"}, "x")
          .IsOk());
  EXPECT_FALSE(
      RunExport(project,
                {.name = "p", .format = "acorn-phrom", .layout = "zip"}, "x")
          .IsOk());
}

TEST(ProjectExportTest, EveryFormatIsListedOnce) {
  std::set<std::string_view> keys;
  for (const ExportFormatInfo& format : ExportFormats()) {
    EXPECT_TRUE(keys.insert(format.key).second) << format.key;
    EXPECT_EQ(FindExportFormat(format.key), &format);
  }
  EXPECT_EQ(keys, (std::set<std::string_view>{"acorn-phrom", "c-header-phrom",
                                              "c-header-speak-external", "wav",
                                              "bbc-dsd"}));
  EXPECT_EQ(FindExportFormat("ssd"), nullptr);
}

TEST(ProjectExportTest, RefusesAnOptionItsFormatDoesNotTake) {
  EXPECT_TRUE(CheckExportPreset({.name = "p",
                                 .format = "c-header-phrom",
                                 .licence = "MIT OR Apache-2.0"})
                  .IsOk());
  EXPECT_TRUE(
      CheckExportPreset({.name = "p", .format = "wav", .audio = "original"})
          .IsOk());
  const Status layout = CheckExportPreset(
      {.name = "p", .format = "wav", .layout = "concatenated"});
  EXPECT_FALSE(layout.IsOk());
  EXPECT_NE(layout.Message().find("layout"), std::string::npos);
  EXPECT_FALSE(CheckExportPreset(
                   {.name = "p", .format = "acorn-phrom", .licence = "MIT"})
                   .IsOk());
  EXPECT_FALSE(
      CheckExportPreset({.name = "p", .format = "c-header-phrom", .audio = "x"})
          .IsOk());
}

TEST(ProjectExportTest, RefusesBadOptionValues) {
  EXPECT_FALSE(
      CheckExportPreset({.name = "p", .format = "wav", .audio = "both"})
          .IsOk());
  EXPECT_FALSE(CheckExportPreset({.name = "p",
                                  .format = "c-header-speak-external",
                                  .licence = "MIT */ int x;"})
                   .IsOk());
  EXPECT_FALSE(
      CheckExportPreset(
          {.name = "p", .format = "wav", .scope = "all", .phrases = {"A"}})
          .IsOk());
}

TEST(ProjectExportTest, RefusesABaseThatIsNotOneFileName) {
  EXPECT_TRUE(CheckExportBase("my words (2)").IsOk());
  for (const char* base : {"", ".", "..", "../up", "a/b", "a\\b", "c:", "a?",
                           "a*", "a|b", "a\nb"}) {
    EXPECT_FALSE(CheckExportBase(base).IsOk()) << base;
  }
  EXPECT_FALSE(
      RunExport(TwoPhroms(), {.name = "p", .format = "acorn-phrom"}, "../x")
          .IsOk());
}

TEST(ProjectExportTest, SelectedScopeTakesTheNamedPhrasesInListOrder) {
  const Project project = TwoPhroms();
  const StatusOr<std::vector<const Phrase*>> phrases =
      PhrasesInScope(project, {.name = "p",
                               .format = "wav",
                               .scope = "selected",
                               .phrases = {"C", "A"}});
  ASSERT_TRUE(phrases.IsOk());
  ASSERT_EQ(phrases->size(), 2U);
  EXPECT_EQ((*phrases)[0]->name, "A");
  EXPECT_EQ((*phrases)[1]->name, "C");
}

TEST(ProjectExportTest, SelectedScopeRefusesANameTheProjectLacks) {
  const StatusOr<std::vector<const Phrase*>> phrases =
      PhrasesInScope(TwoPhroms(), {.name = "p",
                                   .format = "wav",
                                   .scope = "selected",
                                   .phrases = {"A", "NOPE"}});
  ASSERT_FALSE(phrases.IsOk());
  EXPECT_NE(phrases.GetStatus().Message().find("NOPE"), std::string::npos);
}

TEST(ProjectExportTest, SelectedPhrasesAreAllocatedAfresh) {
  // A and C together fit one PHROM, so C becomes word 128 of PHROM 1.
  const StatusOr<std::vector<ExportedFile>> files =
      RunExport(TwoPhroms(),
                {.name = "p",
                 .format = "acorn-phrom",
                 .scope = "selected",
                 .phrases = {"A", "C"}},
                "words");
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  ASSERT_EQ(files->size(), 1U);
  packaging::PhromImage image{.bytes = (*files)[0].bytes, .table = {}};
  EXPECT_EQ(Pointer(image, 'C'), Pointer(image, 128));
  EXPECT_EQ(HeaderStrings(image)[1], "My PHROM 1");
}

TEST(ProjectExportTest, PhromHeaderIsNamedFromTheBase) {
  const StatusOr<std::vector<ExportedFile>> files = RunExport(
      TwoPhroms(), {.name = "p", .format = "c-header-phrom"}, "my words");
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  ASSERT_EQ(files->size(), 1U);
  EXPECT_EQ((*files)[0].name, "my words.h");
  const std::string text((*files)[0].bytes.begin(), (*files)[0].bytes.end());
  EXPECT_NE(text.find("#define MY_WORDS_PHROMS 2\n"), std::string::npos);
  EXPECT_NE(text.find("static const uint8_t my_words_phrom_2["),
            std::string::npos);
  EXPECT_NE(text.find("SPDX-License-Identifier: GPL-3.0-or-later\n"),
            std::string::npos);
}

TEST(ProjectExportTest, SpeakExternalHeaderHasEachPhrasesStream) {
  const Project project = TwoPhroms();
  const StatusOr<std::vector<ExportedFile>> files = RunExport(
      project,
      {.name = "p", .format = "c-header-speak-external", .licence = "CC0-1.0"},
      "words");
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  ASSERT_EQ(files->size(), 1U);
  const std::string text((*files)[0].bytes.begin(), (*files)[0].bytes.end());
  EXPECT_NE(text.find("SPDX-License-Identifier: CC0-1.0\n"), std::string::npos);
  EXPECT_NE(text.find("#define WORDS_WORDS 3\n"), std::string::npos);
  const StatusOr<std::vector<std::uint8_t>> stream =
      packaging::SpeakExternalStream::Build(
          project.Phrases()[2].encoding->frames);
  ASSERT_TRUE(stream.IsOk());
  EXPECT_NE(
      text.find(fmt::format("#define WORDS_LENGTH_C {}\n", stream->size())),
      std::string::npos);
}

TEST(ProjectExportTest, HeadersRefuseAPhraseWithNoFrames) {
  const Project project =
      ProjectWith({EncodedPhrase(1, "A", 100), AudioPhrase(2, "NEW")});
  for (const char* format : {"c-header-phrom", "c-header-speak-external"}) {
    const StatusOr<std::vector<ExportedFile>> files =
        RunExport(project, {.name = "p", .format = format}, "x");
    ASSERT_FALSE(files.IsOk()) << format;
    EXPECT_NE(files.GetStatus().Message().find("NEW"), std::string::npos);
  }
}

TEST(ProjectExportTest, FileNamesAreThoseTheExportWrites) {
  const Project project = TwoPhroms();
  const std::vector<ExportPreset> presets = {
      {.name = "p", .format = "acorn-phrom"},
      {.name = "p", .format = "acorn-phrom", .layout = "concatenated"},
      {.name = "p", .format = "c-header-phrom"},
      {.name = "p", .format = "c-header-speak-external"},
      {.name = "p", .format = "bbc-dsd"}};
  for (const ExportPreset& preset : presets) {
    const StatusOr<std::vector<ExportedFile>> files =
        RunExport(project, preset, "words");
    ASSERT_TRUE(files.IsOk()) << preset.format;
    std::vector<std::string> names;
    for (const ExportedFile& file : *files) {
      names.push_back(file.name);
    }
    const StatusOr<std::vector<std::string>> planned =
        ExportFileNames(project, preset, "words");
    ASSERT_TRUE(planned.IsOk()) << preset.format;
    EXPECT_EQ(*planned, names) << preset.format;
  }
  const StatusOr<std::vector<std::string>> wav = ExportFileNames(
      ProjectWith({AudioPhrase(1, "ONE,WON"), AudioPhrase(2, "ONE WON")}),
      {.name = "p", .format = "wav", .audio = "original"}, "w");
  ASSERT_TRUE(wav.IsOk());
  EXPECT_EQ(*wav,
            (std::vector<std::string>{"w_ONE_WON.wav", "w_ONE_WON_2.wav"}));
}

TEST(ProjectExportTest, FileNamesFailAsTheExportWould) {
  const Project project =
      ProjectWith({EncodedPhrase(1, "A", 100), AudioPhrase(2, "NEW"),
                   LockedPhrase(3, "LOCKED", 100)});
  EXPECT_FALSE(
      ExportFileNames(project, {.name = "p", .format = "wav"}, "x").IsOk());
  EXPECT_FALSE(ExportFileNames(project,
                               {.name = "p",
                                .format = "wav",
                                .scope = "selected",
                                .phrases = {"LOCKED"},
                                .audio = "original"},
                               "x")
                   .IsOk());
  EXPECT_FALSE(
      ExportFileNames(project, {.name = "p", .format = "acorn-phrom"}, "x")
          .IsOk());
  EXPECT_FALSE(
      ExportFileNames(project, {.name = "p", .format = "wav"}, "a/b").IsOk());
}

TEST(ProjectExportTest, EncodingsMustBeCurrentExceptForTheOriginalAudio) {
  const Project project = ProjectWith(
      {LockedPhrase(1, "LOCKED", 100), EncodedPhrase(2, "OLD", 100)});
  const Status stale =
      CheckEncodingsCurrent(project, {.name = "p", .format = "acorn-phrom"});
  ASSERT_FALSE(stale.IsOk());
  EXPECT_NE(stale.Message().find("OLD"), std::string::npos);
  EXPECT_TRUE(CheckEncodingsCurrent(project, {.name = "p",
                                              .format = "acorn-phrom",
                                              .scope = "selected",
                                              .phrases = {"LOCKED"}})
                  .IsOk());
  EXPECT_TRUE(CheckEncodingsCurrent(
                  project, {.name = "p", .format = "wav", .audio = "original"})
                  .IsOk());
}

class ProjectWavExportTest : public ::testing::Test {
 protected:
  ExportTools Tools() { return {.synth = &synth_, .wav_codec = &codec_}; }

  ::testing::StrictMock<testing::MockSynth> synth_;
  ::testing::StrictMock<testing::MockWavCodec> codec_;
};

TEST_F(ProjectWavExportTest, EncodedIsTheSpeakerPinAt16Bits) {
  const Project project = ProjectWith({EncodedPhrase(1, "ONE,WON", 100)});
  const StatusOr<std::vector<std::uint8_t>> packed =
      lpc::FrameCodec::Pack(project.Phrases()[0].encoding->frames);
  ASSERT_TRUE(packed.IsOk());
  synth::SynthResult spoken;
  // 200 codes is beyond SPEAKER's +127, so it is clipped.
  spoken.samples = {-64, 0, 64 * 10, 64 * 200};
  EXPECT_CALL(synth_, Synthesize(::testing::ElementsAreArray(*packed),
                                 synth::SynthPath::kVsm))
      .WillOnce(::testing::Return(spoken));
  const std::vector<float> expected = {-64 / 32768.0F, 0.0F, 640 / 32768.0F,
                                       (64 * 127) / 32768.0F};
  EXPECT_CALL(codec_, Encode(::testing::ElementsAreArray(expected), 8000,
                             audio::WavOutputFormat::kPcm16))
      .WillOnce(::testing::Return(std::vector<std::uint8_t>{1, 2, 3}));
  const StatusOr<std::vector<ExportedFile>> files =
      RunExport(project, {.name = "p", .format = "wav"}, "words", Tools());
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  ASSERT_EQ(files->size(), 1U);
  EXPECT_EQ((*files)[0],
            (ExportedFile{.name = "words_ONE_WON.wav", .bytes = {1, 2, 3}}));
}

TEST_F(ProjectWavExportTest, OriginalIsTheSourceAt24Bits) {
  const Project project =
      ProjectWith({AudioPhrase(1, "A"), AudioPhrase(2, "B")});
  EXPECT_CALL(codec_,
              Encode(::testing::ElementsAreArray(project.Phrases()[1].audio),
                     8000, audio::WavOutputFormat::kPcm24))
      .WillOnce(::testing::Return(std::vector<std::uint8_t>{9}));
  const StatusOr<std::vector<ExportedFile>> files =
      RunExport(project,
                {.name = "p",
                 .format = "wav",
                 .scope = "selected",
                 .phrases = {"B"},
                 .audio = "original"},
                "src", Tools());
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  ASSERT_EQ(files->size(), 1U);
  EXPECT_EQ((*files)[0], (ExportedFile{.name = "src_B.wav", .bytes = {9}}));
}

TEST_F(ProjectWavExportTest, OriginalRefusesAPhraseWithNoAudio) {
  const Project project = ProjectWith({LockedPhrase(1, "LOCKED", 100)});
  const StatusOr<std::vector<ExportedFile>> files =
      RunExport(project, {.name = "p", .format = "wav", .audio = "original"},
                "x", Tools());
  ASSERT_FALSE(files.IsOk());
  EXPECT_NE(files.GetStatus().Message().find("LOCKED"), std::string::npos);
}

TEST_F(ProjectWavExportTest, NeedsTheTools) {
  const Project project = ProjectWith({AudioPhrase(1, "A")});
  EXPECT_EQ(RunExport(project, {.name = "p", .format = "wav"}, "x")
                .GetStatus()
                .Code(),
            StatusCode::kFailedPrecondition);
}

}  // namespace
}  // namespace speak5220::core::project
