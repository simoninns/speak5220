/*
 * File:        project_serializer_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the project's YAML files, with a mocked WAV
 *              codec and project store: quoting, unknown keys, type checks,
 *              versions and unsafe paths
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_serializer.h"

#include <bit>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/audio/wav_io.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_store.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"

namespace speak5220::core::project {
namespace {

using ::testing::_;
using testing::AudioPhrase;
using testing::EncodedPhrase;
using ::testing::HasSubstr;
using ::testing::Invoke;
using testing::LockedPhrase;
using testing::MockProjectStore;
using testing::MockWavCodec;
using ::testing::NiceMock;
using testing::ProjectWith;
using ::testing::Return;
using testing::TestId;

// The mocked codec stores each float's bits, which is enough to check
// that audio reaches the right file and comes back.
std::vector<std::uint8_t> FakeWav(std::span<const float> samples) {
  std::vector<std::uint8_t> bytes = {'F', 'A', 'K', 'E'};
  for (const float sample : samples) {
    const auto bits = std::bit_cast<std::uint32_t>(sample);
    for (unsigned shift = 0; shift < 32; shift += 8) {
      bytes.push_back(static_cast<std::uint8_t>(bits >> shift));
    }
  }
  return bytes;
}

audio::DecodedWav FromFakeWav(std::span<const std::uint8_t> bytes) {
  audio::DecodedWav wav{.sample_rate = kProjectSampleRate,
                        .channels = 1,
                        .format = audio::WavSampleFormat::kPcm24,
                        .samples = {}};
  for (std::size_t i = 4; i + 4 <= bytes.size(); i += 4) {
    std::uint32_t bits = 0;
    for (unsigned j = 0; j < 4; ++j) {
      bits |= static_cast<std::uint32_t>(bytes[i + j]) << (8U * j);
    }
    wav.samples.push_back(std::bit_cast<float>(bits));
  }
  return wav;
}

class ProjectSerializerTest : public ::testing::Test {
 protected:
  ProjectSerializerTest() {
    ON_CALL(codec_, Encode(_, _, _))
        .WillByDefault(Invoke(
            [](std::span<const float> samples, int, audio::WavOutputFormat) {
              return StatusOr<std::vector<std::uint8_t>>(FakeWav(samples));
            }));
    ON_CALL(codec_, Decode(_))
        .WillByDefault(Invoke([](std::span<const std::uint8_t> bytes) {
          return StatusOr<audio::DecodedWav>(FromFakeWav(bytes));
        }));
  }

  ProjectFiles Save(const Project& project) {
    StatusOr<ProjectFiles> files = serializer_.ToFiles(project);
    EXPECT_TRUE(files.IsOk()) << files.GetStatus().ToString();
    return files.IsOk() ? *std::move(files) : ProjectFiles{};
  }

  std::string ProjectYamlOf(const Project& project) {
    const ProjectFiles files = Save(project);
    const auto found = files.find(std::string(kProjectYamlPath));
    return found == files.end()
               ? std::string()
               : std::string(found->second.begin(), found->second.end());
  }

  StatusOr<Project> Load(const ProjectFiles& files) {
    return serializer_.FromFiles(files);
  }

  // A project.yaml with `phrases` (YAML text, indented as list items) and
  // nothing else.
  static ProjectFiles WithProjectYaml(std::string_view text) {
    return {{std::string(kProjectYamlPath),
             std::vector<std::uint8_t>(text.begin(), text.end())}};
  }

  NiceMock<MockWavCodec> codec_;
  ProjectSerializer serializer_{codec_};
};

std::vector<std::uint8_t> Bytes(std::string_view text) {
  return {text.begin(), text.end()};
}

std::string PhraseFile(int n, std::string_view name) {
  return "phrases/" + TestId(n) + "/" + std::string(name);
}

// A phrase entry for hand-written project.yaml files.
std::string PhraseYaml(int n, std::string_view extra = "") {
  return "  - id: \"" + TestId(n) + "\"\n    name: \"P" + std::to_string(n) +
         "\"\n    locked: true\n" + std::string(extra);
}

TEST_F(ProjectSerializerTest, SavedProjectLoadsUnchanged) {
  Phrase encoded = EncodedPhrase(1, "HELLO", 40);
  encoded.description = "Greeting, recorded 2026-10-10 \xc2\xb7 caf\xc3\xa9";
  encoded.encoder = {{"target_rms", "18"},
                     {"pre_emphasis", "0.9375"},
                     {"target_peak", "none"},
                     {"repeats", "false"}};
  encoded.encoding->pesq = 2.08;
  encoded.encoding->anchor = 2.31;
  encoded.encoding->frames = {
      lpc::Frame::Voiced(9, 34, {12, 20, 8, 7, 9, 8, 7, 4, 3, 2}),
      lpc::Frame::Repeat(9, 35), lpc::Frame::Unvoiced(4, {20, 18, 9, 6}),
      lpc::Frame::Silence(), lpc::Frame::Stop()};
  encoded.play_start = 2;
  Phrase locked = LockedPhrase(2, "O'CLOCK", 20);
  Project project = ProjectWith({encoded, locked, AudioPhrase(3, "NO")});
  ProjectSettings settings = project.Settings();
  settings.name = "My PHROM";
  settings.header_strings = {"(C)2026 Simon Inns", "My PHROM", "1.00"};
  settings.ascii_map = {{'A', "HELLO"}, {' ', "NO"}, {':', "O'CLOCK"}};
  settings.default_encoder.target_peak = 100.0;
  settings.default_encoder.voicing_threshold = 0.35;
  settings.phroms = {{.number = 2,
                      .header_strings = {"(C)2026 Simon Inns", "Second", "1"},
                      .unknown = {}}};
  ASSERT_TRUE(project.SetSettings(settings).IsOk());
  ASSERT_TRUE(project
                  .SetExports({{.name = "phrom",
                                .format = "acorn-phrom",
                                .scope = "all",
                                .layout = "separate",
                                .unknown = {}}})
                  .IsOk());

  const ProjectFiles files = Save(project);
  EXPECT_TRUE(files.contains(PhraseFile(1, "source.wav")));
  EXPECT_TRUE(files.contains(PhraseFile(1, "frames.yaml")));
  EXPECT_FALSE(files.contains(PhraseFile(2, "source.wav")));
  EXPECT_TRUE(files.contains(PhraseFile(2, "frames.yaml")));
  EXPECT_FALSE(files.contains(PhraseFile(3, "frames.yaml")));
  const StatusOr<Project> loaded = Load(files);
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(*loaded, project);
  // And saving the loaded project gives the same files.
  EXPECT_EQ(Save(*loaded), files);
}

TEST_F(ProjectSerializerTest, AudioGoesThroughTheCodecAs24BitAt8kHz) {
  MockWavCodec codec;
  ProjectSerializer serializer(codec);
  const Project project = ProjectWith({AudioPhrase(1, "A")});
  EXPECT_CALL(codec,
              Encode(_, kProjectSampleRate, audio::WavOutputFormat::kPcm24))
      .WillOnce(Return(Bytes("wav bytes")));
  const StatusOr<ProjectFiles> files = serializer.ToFiles(project);
  ASSERT_TRUE(files.IsOk());
  EXPECT_EQ(files->at(PhraseFile(1, "source.wav")), Bytes("wav bytes"));
}

TEST_F(ProjectSerializerTest, StringsAreAlwaysQuoted) {
  const std::vector<std::string> names = {
      "NO",  "YES",    "ON",       "OFF",     "1",
      "000", "-0.125", "(TONE 1)", "O'CLOCK", "TWO,TO,TOO"};
  std::vector<Phrase> phrases;
  phrases.reserve(names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    phrases.push_back(AudioPhrase(static_cast<int>(i) + 1, names[i]));
  }
  Project project = ProjectWith(std::move(phrases));
  ProjectSettings settings = project.Settings();
  settings.name = "true";
  settings.ascii_map = {{' ', "NO"}, {':', "YES"}, {'1', "1"}};
  ASSERT_TRUE(project.SetSettings(settings).IsOk());
  const std::string yaml = ProjectYamlOf(project);
  for (const std::string& name : names) {
    EXPECT_THAT(yaml, HasSubstr("name: \"" + name + "\"")) << name;
  }
  EXPECT_THAT(yaml, HasSubstr("name: \"true\""));
  EXPECT_THAT(yaml, HasSubstr("\" \": \"NO\""));
  EXPECT_THAT(yaml, HasSubstr("\":\": \"YES\""));
  EXPECT_THAT(yaml, HasSubstr("\"1\": \"1\""));
  EXPECT_THAT(yaml, HasSubstr("format: \"speak5220-project\""));
  const StatusOr<Project> loaded = Load(Save(project));
  ASSERT_TRUE(loaded.IsOk());
  EXPECT_EQ(*loaded, project);
}

TEST_F(ProjectSerializerTest, OptionsAreWrittenWithTheirTypes) {
  Project project = ProjectWith({AudioPhrase(1, "A")});
  ASSERT_TRUE(
      project
          .SetEncoding(TestId(1), {.overrides = {{"target_rms", "18"},
                                                 {"repeats", "false"},
                                                 {"pre_emphasis", "chirp"}}})
          .IsOk());
  const std::string yaml = ProjectYamlOf(project);
  EXPECT_THAT(yaml, HasSubstr("    target_rms: 20\n    target_peak: null\n"));
  EXPECT_THAT(yaml, HasSubstr("    repeats: true\n"));
  EXPECT_THAT(yaml, HasSubstr("    pre_emphasis: \"chirp\"\n"));
  EXPECT_THAT(yaml, HasSubstr("    voicing_threshold: 0.3\n"));
  EXPECT_THAT(yaml, HasSubstr("    encoder:\n      target_rms: 18\n"
                              "      repeats: false\n"
                              "      pre_emphasis: \"chirp\"\n"));
}

TEST_F(ProjectSerializerTest, FramesAreWrittenOnePerLine) {
  Phrase phrase = AudioPhrase(1, "A");
  phrase.encoding = CachedEncoding{
      .encoder_version = "1.0.0",
      .settings_hash = "abc",
      .frames = {lpc::Frame::Voiced(9, 34, {12, 20, 8, 7, 9, 8, 7, 4, 3, 2}),
                 lpc::Frame::Repeat(9, 35),
                 lpc::Frame::Unvoiced(4, {20, 18, 9, 6}), lpc::Frame::Silence(),
                 lpc::Frame::Stop()},
      .pesq = 2.08,
      .anchor = std::nullopt,
      .unknown = {},
      .quality_unknown = {}};
  const ProjectFiles files = Save(ProjectWith({phrase}));
  const std::vector<std::uint8_t>& bytes =
      files.at(PhraseFile(1, "frames.yaml"));
  EXPECT_EQ(std::string(bytes.begin(), bytes.end()),
            "encoder_version: \"1.0.0\"\n"
            "settings_hash: \"abc\"\n"
            "frames:\n"
            "  - {energy: 9, repeat: 0, pitch: 34, k: [12, 20, 8, 7, 9, 8, 7, "
            "4, 3, 2]}\n"
            "  - {energy: 9, repeat: 1, pitch: 35}\n"
            "  - {energy: 4, repeat: 0, pitch: 0, k: [20, 18, 9, 6]}\n"
            "  - {energy: 0}\n"
            "  - {energy: 15}\n"
            "quality:\n"
            "  pesq: 2.08\n");
}

TEST_F(ProjectSerializerTest, UnknownKeysAreKeptAndWrittenBack) {
  const std::string project_yaml =
      "format: \"speak5220-project\"\n"
      "version: 1\n"
      "future_root: {a: [1, \"2\"], b: ~}\n"
      "project:\n"
      "  name: \"P\"\n"
      "  theme: \"dark\"\n"
      "  default_encoder:\n"
      "    target_rms: 20\n"
      "    future_option: 7\n"
      "  phroms:\n"
      "    - number: 1\n"
      "      header_strings: [\"(C)\", \"X\", \"1\"]\n"
      "      colour: \"NO\"\n"
      "phrases:\n"
      "  - id: \"" +
      TestId(1) +
      "\"\n"
      "    name: \"A\"\n"
      "    source:\n"
      "      file: \"a.wav\"\n"
      "      rate: 8000\n"
      "      bits: 24\n"
      "    encoder:\n"
      "      gate_db: 40\n"
      "      new_thing: \"on\"\n"
      "    rating: 5\n"
      "exports:\n"
      "  - name: \"e\"\n"
      "    format: \"acorn-phrom\"\n"
      "    licence: \"MIT\"\n";
  const std::string frames_yaml =
      "encoder_version: \"1.0.0\"\n"
      "settings_hash: \"h\"\n"
      "frames:\n"
      "  - {energy: 15}\n"
      "quality:\n"
      "  pesq: 2.5\n"
      "  stoi: 0.9\n"
      "notes: \"YES\"\n";
  ProjectFiles files = WithProjectYaml(project_yaml);
  files[PhraseFile(1, "source.wav")] = FakeWav(std::vector<float>{0.5F});
  files[PhraseFile(1, "frames.yaml")] = Bytes(frames_yaml);
  files[PhraseFile(1, "notes.txt")] = Bytes("phrase notes");
  files["README.txt"] = Bytes("project notes");

  const StatusOr<Project> loaded = Load(files);
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  const Phrase& phrase = loaded->Phrases()[0];
  EXPECT_EQ(phrase.unknown, (UnknownKeys{{"rating", "5"}}));
  EXPECT_EQ(phrase.source->unknown, (UnknownKeys{{"bits", "24"}}));
  EXPECT_EQ(phrase.encoder_unknown, (UnknownKeys{{"new_thing", "\"on\""}}));
  EXPECT_EQ(phrase.encoder.at("gate_db"), "40");
  EXPECT_EQ(loaded->Settings().default_encoder_unknown,
            (UnknownKeys{{"future_option", "7"}}));
  EXPECT_EQ(phrase.extra_files.at("notes.txt"), Bytes("phrase notes"));
  EXPECT_EQ(loaded->Contents().extra_files.at("README.txt"),
            Bytes("project notes"));

  const ProjectFiles saved = Save(*loaded);
  const std::vector<std::uint8_t>& yaml_bytes =
      saved.at(std::string(kProjectYamlPath));
  const std::string yaml(yaml_bytes.begin(), yaml_bytes.end());
  EXPECT_THAT(yaml, HasSubstr("future_root: {a: [1, \"2\"], b: null}"));
  EXPECT_THAT(yaml, HasSubstr("  theme: \"dark\"\n"));
  EXPECT_THAT(yaml, HasSubstr("    future_option: 7\n"));
  EXPECT_THAT(yaml, HasSubstr("      colour: \"NO\"\n"));
  EXPECT_THAT(yaml, HasSubstr("      bits: 24\n"));
  EXPECT_THAT(yaml, HasSubstr("      new_thing: \"on\"\n"));
  EXPECT_THAT(yaml, HasSubstr("    rating: 5\n"));
  EXPECT_THAT(yaml, HasSubstr("    licence: \"MIT\"\n"));
  const std::vector<std::uint8_t>& frames_bytes =
      saved.at(PhraseFile(1, "frames.yaml"));
  const std::string frames(frames_bytes.begin(), frames_bytes.end());
  EXPECT_THAT(frames, HasSubstr("  stoi: 0.9\n"));
  EXPECT_THAT(frames, HasSubstr("notes: \"YES\"\n"));
  EXPECT_EQ(saved.at(PhraseFile(1, "notes.txt")), Bytes("phrase notes"));
  EXPECT_EQ(saved.at("README.txt"), Bytes("project notes"));

  // A second round trip changes nothing.
  const StatusOr<Project> reloaded = Load(saved);
  ASSERT_TRUE(reloaded.IsOk()) << reloaded.GetStatus().ToString();
  EXPECT_EQ(*reloaded, *loaded);
}

TEST_F(ProjectSerializerTest, RefusesFilesThatAreNotAProject) {
  EXPECT_EQ(Load({}).GetStatus().Code(), StatusCode::kNotFound);
  EXPECT_FALSE(Load(WithProjectYaml("format: \"other\"\nversion: 1\n")).IsOk());
  EXPECT_FALSE(Load(WithProjectYaml("[1, 2]\n")).IsOk());
  const StatusOr<Project> broken = Load(WithProjectYaml("a: [1, 2\n"));
  ASSERT_FALSE(broken.IsOk());
  EXPECT_THAT(broken.GetStatus().Message(), HasSubstr("not valid YAML"));
}

TEST_F(ProjectSerializerTest, RefusesAProjectFromANewerVersion) {
  const StatusOr<Project> loaded =
      Load(WithProjectYaml("format: \"speak5220-project\"\nversion: 2\n"));
  ASSERT_FALSE(loaded.IsOk());
  EXPECT_EQ(loaded.GetStatus().Code(), StatusCode::kFailedPrecondition);
  EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr("newer version"));
}

TEST_F(ProjectSerializerTest, LoadsTheCurrentVersionWithDefaults) {
  const StatusOr<Project> loaded =
      Load(WithProjectYaml("format: \"speak5220-project\"\nversion: 1\n"));
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(*loaded, Project());
}

TEST_F(ProjectSerializerTest, ChecksTheTypeOfEveryValue) {
  const std::string head = "format: \"speak5220-project\"\nversion: 1\n";
  struct Case {
    std::string yaml;
    std::string where;
  };
  const std::vector<Case> cases = {
      {head + "version_x: 1\nphrases: {a: 1}\n", "phrases must be a list"},
      {head + "phrases:\n  - id: \"" + TestId(1) + "\"\n    name: [1]\n",
       "phrases[0].name must be a string"},
      {head + "phrases:\n" + PhraseYaml(1, "    play_start: \"3\"\n"),
       "phrases[0].play_start must be a whole number"},
      {head + "phrases:\n" + PhraseYaml(1, "    play_start: -1\n"),
       "phrases[0].play_start must be from 0"},
      {head + "phrases:\n  - id: \"" + TestId(1) +
           "\"\n    name: \"A\"\n    locked: yes\n",
       "phrases[0].locked must be true or false"},
      {head + "phrases:\n" + PhraseYaml(1, "    source: {rate: 4.5}\n"),
       "phrases[0].source.rate must be a whole number"},
      {head + "project:\n  default_encoder:\n    target_rms: \"18\"\n",
       "project.default_encoder.target_rms must not be quoted"},
      {head + "project:\n  default_encoder:\n    target_rms: 500\n",
       "target_rms is 500"},
      {head + "project:\n  default_encoder:\n    repeats: 1\n",
       "repeats must be true or false"},
      {head + "project:\n  ascii_map: {\"AB\": \"X\"}\n",
       "each key is one character"},
      {head + "project:\n  header_strings: \"one\"\n",
       "project.header_strings must be a list of strings"},
      {head + "project: {name: \"a\", name: \"b\"}\n",
       "project.name is given twice"},
      {head + "phrases:\n  - name: \"A\"\n", "phrases[0] has no id"},
  };
  for (const Case& c : cases) {
    const StatusOr<Project> loaded = Load(WithProjectYaml(c.yaml));
    ASSERT_FALSE(loaded.IsOk()) << c.yaml;
    EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr(c.where)) << c.yaml;
  }
}

TEST_F(ProjectSerializerTest, RefusesAPhraseIdThatIsNotAUuid) {
  for (const char* id : {"../../etc", "A0000000-0000-0000-0000-000000000001",
                         "00000000-0000-0000-0000-00000000000g", "x/y"}) {
    const std::string yaml = std::string(
                                 "format: \"speak5220-project\"\n"
                                 "version: 1\nphrases:\n  - id: \"") +
                             id + "\"\n    name: \"A\"\n    locked: true\n";
    const StatusOr<Project> loaded = Load(WithProjectYaml(yaml));
    ASSERT_FALSE(loaded.IsOk()) << id;
    EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr("UUID")) << id;
  }
}

TEST_F(ProjectSerializerTest, PhraseWithASourceNeedsItsAudio) {
  const std::string yaml =
      "format: \"speak5220-project\"\nversion: 1\nphrases:\n  - id: \"" +
      TestId(1) + "\"\n    name: \"A\"\n    source: {file: \"a.wav\"}\n";
  const StatusOr<Project> loaded = Load(WithProjectYaml(yaml));
  ASSERT_FALSE(loaded.IsOk());
  EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr("source.wav"));
}

TEST_F(ProjectSerializerTest, AudioMustBe8kHzMono) {
  ProjectFiles files = Save(ProjectWith({AudioPhrase(1, "A")}));
  EXPECT_CALL(codec_, Decode(_))
      .WillOnce(
          Return(audio::DecodedWav{.sample_rate = 44100,
                                   .channels = 1,
                                   .format = audio::WavSampleFormat::kPcm16,
                                   .samples = {0.0F}}));
  const StatusOr<Project> loaded = Load(files);
  ASSERT_FALSE(loaded.IsOk());
  EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr("8000 Hz mono"));
}

TEST_F(ProjectSerializerTest, ChecksEveryFrame) {
  const std::string yaml =
      "format: \"speak5220-project\"\nversion: 1\n"
      "phrases:\n" +
      PhraseYaml(1);
  const std::vector<std::string> bad_frames = {
      "frames: [{energy: 16}]",
      "frames: [{repeat: 1}]",
      "frames: [{energy: 3, pitch: 0, k: [1, 2, 3, 4, 5]}]",
      "frames: [{energy: 3, repeat: 2, pitch: 4}]",
      "frames: [{energy: 3, volume: 4}]",
      "frames: [{energy: 3, k: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]}]",
      "frames: [{energy: 3, pitch: 64}]",
      "settings_hash: \"x\"",
  };
  for (const std::string& frames : bad_frames) {
    ProjectFiles files = WithProjectYaml(yaml);
    files[PhraseFile(1, "frames.yaml")] = Bytes(frames + "\n");
    EXPECT_FALSE(Load(files).IsOk()) << frames;
  }
}

TEST_F(ProjectSerializerTest, SaveAndLoadGoThroughTheStore) {
  const Project project = ProjectWith({AudioPhrase(1, "A")});
  const ProjectFiles files = Save(project);
  MockProjectStore store;
  EXPECT_CALL(store, Write(files)).WillOnce(Return(OkStatus()));
  EXPECT_TRUE(serializer_.Save(project, store).IsOk());
  EXPECT_CALL(store, Read()).WillOnce(Return(files));
  const StatusOr<Project> loaded = serializer_.Load(store);
  ASSERT_TRUE(loaded.IsOk());
  EXPECT_EQ(*loaded, project);
}

TEST_F(ProjectSerializerTest, StoreFailuresArePassedOn) {
  MockProjectStore store;
  EXPECT_CALL(store, Write(_)).WillOnce(Return(UnavailableError("disk full")));
  EXPECT_EQ(serializer_.Save(Project(), store).Message(), "disk full");
  EXPECT_CALL(store, Read()).WillOnce(Return(NotFoundError("gone")));
  EXPECT_EQ(serializer_.Load(store).GetStatus().Message(), "gone");
}

}  // namespace
}  // namespace speak5220::core::project
