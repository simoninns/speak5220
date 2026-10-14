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
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_store.h"
#include "core/quality/quality_assessor.h"
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
using ::testing::Not;
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

// Every option of `stage` at its default, as the entries of its mapping
// (default_preprocess or default_encoder) in a hand-written project.yaml,
// which must list them all.
std::string EveryOptionYaml(encoder::OptionStage stage) {
  std::string yaml;
  for (const encoder::EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    if (info.stage == stage) {
      yaml +=
          "    " + std::string(info.field) + ": " +
          encoder::EncoderOptionText(encoder::EncoderOptions{}, info.field) +
          "\n";
    }
  }
  return yaml;
}

// Both options mappings, every option at its default; default_encoder
// last, so entries added after it go into it.
std::string EveryOptionYaml() {
  return "  default_preprocess:\n" +
         EveryOptionYaml(encoder::OptionStage::kPreprocess) +
         "  default_encoder:\n" +
         EveryOptionYaml(encoder::OptionStage::kEncoder);
}

// The head of a hand-written project.yaml that loads as Project().
std::string MinimalProjectYaml() {
  return "format: \"speak5220-project\"\nversion: 1\nproject:\n" +
         EveryOptionYaml();
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
                     {"noise_gate_db", "none"},
                     {"resonance_hold_db", "20"}};
  encoded.encoding->quality =
      CachedQuality{.measures_version = quality::kMeasuresVersion,
                    .lsd_db = 3.25,
                    .lsd_over_2db = 81.5,
                    .lsd_over_4db = 27.75,
                    .gross_pitch_error = 5.5,
                    .f0_rmse_cents = 60.125,
                    .voicing_error = 9.875,
                    .estoi = 0.75,
                    .score = 0.6875};
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
  settings.default_encoder.preprocess.noise_gate_db = 30.0;
  settings.default_encoder.voicing_threshold = 0.35;
  settings.phroms = {{.number = 2,
                      .header_strings = {"(C)2026 Simon Inns", "Second", "1"},
                      .unknown = {}}};
  ASSERT_TRUE(project.SetSettings(settings).IsOk());
  ASSERT_TRUE(
      project
          .SetExports({{.name = "phrom",
                        .format = "acorn-phrom",
                        .scope = "all",
                        .layout = "separate",
                        .unknown = {}},
                       {.name = "words",
                        .format = "c-header-speak-external",
                        .scope = "selected",
                        .phrases = {"NO", "O'CLOCK"},
                        .licence = "CC0-1.0"},
                       {.name = "chip", .format = "wav", .audio = "original"}})
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

TEST_F(ProjectSerializerTest, PresetsWriteOnlyTheOptionsTheySet) {
  Project project = ProjectWith({AudioPhrase(1, "NO")});
  ASSERT_TRUE(project
                  .SetExports({{.name = "words",
                                .format = "c-header-phrom",
                                .scope = "selected",
                                .phrases = {"NO"},
                                .licence = "MIT"},
                               {.name = "chip", .format = "wav"}})
                  .IsOk());
  const std::string yaml = ProjectYamlOf(project);
  EXPECT_THAT(yaml, HasSubstr("  - name: \"words\"\n"
                              "    format: \"c-header-phrom\"\n"
                              "    scope: \"selected\"\n"
                              "    phrases: [\"NO\"]\n"
                              "    licence: \"MIT\"\n"));
  EXPECT_THAT(yaml, HasSubstr("  - name: \"chip\"\n"
                              "    format: \"wav\"\n"
                              "    scope: \"all\"\n"));
  EXPECT_THAT(yaml, Not(HasSubstr("audio:")));
  EXPECT_THAT(yaml, Not(HasSubstr("layout:")));
}

TEST_F(ProjectSerializerTest, RefusesPresetPhrasesThatAreNotAList) {
  const std::string yaml = MinimalProjectYaml() +
                           "exports:\n"
                           "  - name: \"e\"\n"
                           "    format: \"wav\"\n"
                           "    phrases: \"NO\"\n";
  const StatusOr<Project> loaded = Load(WithProjectYaml(yaml));
  ASSERT_FALSE(loaded.IsOk());
  EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr("phrases"));
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
                                                 {"window", "200"},
                                                 {"pre_emphasis", "chirp"}}})
          .IsOk());
  const std::string yaml = ProjectYamlOf(project);
  EXPECT_THAT(yaml, HasSubstr("    target_rms: 20\n    repeat_lambda: 300\n"));
  EXPECT_THAT(yaml, HasSubstr("    window: 256\n"));
  EXPECT_THAT(yaml, HasSubstr("    pre_emphasis: \"chirp\"\n"));
  EXPECT_THAT(yaml, HasSubstr("    voicing_threshold: 0.6\n"));
  EXPECT_THAT(yaml, HasSubstr("    encoder:\n      target_rms: 18\n"
                              "      pre_emphasis: \"chirp\"\n"
                              "      window: 200\n"));
}

// Every option is written, so a project never depends on the defaults:
// the preprocessing's under default_preprocess, the noise gate, unset, as
// null; the encoder's under default_encoder.
TEST_F(ProjectSerializerTest, EveryOptionIsWrittenTheNoiseGateUnsetAsNull) {
  Project project = ProjectWith({AudioPhrase(1, "A")});
  const std::string defaults = ProjectYamlOf(project);
  EXPECT_THAT(defaults, HasSubstr("  default_preprocess:\n"
                                  "    noise_gate_db: null\n"
                                  "    noise_gate_ms: 100\n"
                                  "    bass_cut_hz: null\n"
                                  "    treble_tilt: 0\n"
                                  "    de_ess_db: null\n"
                                  "  default_encoder:\n"
                                  "    target_rms: 20\n"));
  EXPECT_THAT(defaults, HasSubstr("    voicing_evidence: 5\n"
                                  "    voicing_min_run: 2\n"
                                  "    level_agc: 1.25\n"));
  EXPECT_THAT(defaults, HasSubstr("    lag_window_hz: 60\n"
                                  "    resonance_hold_db: 14\n"));
  ProjectSettings settings = project.Settings();
  settings.default_encoder.preprocess.noise_gate_db = 30.0;
  ASSERT_TRUE(project.SetSettings(settings).IsOk());
  ASSERT_TRUE(
      project.SetEncoding(TestId(1), {.overrides = {{"noise_gate_db", "none"}}})
          .IsOk());
  const std::string yaml = ProjectYamlOf(project);
  EXPECT_THAT(yaml,
              HasSubstr("  default_preprocess:\n    noise_gate_db: 30\n"));
  EXPECT_THAT(yaml, HasSubstr("    preprocess:\n      noise_gate_db: null\n"));
  EXPECT_THAT(yaml, Not(HasSubstr("    encoder:\n")));
  const StatusOr<Project> loaded = Load(Save(project));
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(*loaded, project);
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
      .quality = CachedQuality{.measures_version = quality::kMeasuresVersion,
                               .lsd_db = 3.25,
                               .lsd_over_2db = 81.5,
                               .lsd_over_4db = 27.75,
                               .gross_pitch_error = 5.5,
                               .f0_rmse_cents = 60.125,
                               .voicing_error = 9.875,
                               .estoi = std::nullopt,
                               .score = 0.6875},
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
            "  measures_version: 1\n"
            "  lsd_db: 3.25\n"
            "  lsd_over_2db: 81.5\n"
            "  lsd_over_4db: 27.75\n"
            "  gross_pitch_error: 5.5\n"
            "  f0_rmse_cents: 60.125\n"
            "  voicing_error: 9.875\n"
            "  score: 0.6875\n");
}

TEST_F(ProjectSerializerTest, QualityOfOtherMeasuresIsMeasuredAgain) {
  Phrase phrase = EncodedPhrase(1, "A", 20);
  phrase.encoding->quality =
      CachedQuality{.measures_version = quality::kMeasuresVersion,
                    .lsd_db = 3.0,
                    .lsd_over_2db = 80.0,
                    .lsd_over_4db = 25.0,
                    .gross_pitch_error = 5.0,
                    .f0_rmse_cents = 60.0,
                    .voicing_error = 10.0,
                    .estoi = 0.75,
                    .score = 0.7};
  ProjectFiles files = Save(ProjectWith({phrase}));
  const StatusOr<Project> same = Load(files);
  ASSERT_TRUE(same.IsOk()) << same.GetStatus().ToString();
  EXPECT_EQ(same->Phrases()[0].encoding->quality, phrase.encoding->quality);

  // Written by other measures: dropped, to be measured again.
  std::vector<std::uint8_t>& bytes = files.at(PhraseFile(1, "frames.yaml"));
  std::string text(bytes.begin(), bytes.end());
  const std::string version = "measures_version: 1";
  ASSERT_NE(text.find(version), std::string::npos);
  text.replace(text.find(version), version.size(), "measures_version: 0");
  bytes = Bytes(text);
  const StatusOr<Project> older = Load(files);
  ASSERT_TRUE(older.IsOk()) << older.GetStatus().ToString();
  EXPECT_FALSE(older->Phrases()[0].encoding->quality.has_value());
}

TEST_F(ProjectSerializerTest, UnknownKeysAreKeptAndWrittenBack) {
  const std::string project_yaml =
      "format: \"speak5220-project\"\n"
      "version: 1\n"
      "future_root: {a: [1, \"2\"], b: ~}\n"
      "project:\n"
      "  name: \"P\"\n"
      "  theme: \"dark\"\n"
      "  default_preprocess:\n" +
      EveryOptionYaml(encoder::OptionStage::kPreprocess) +
      "    future_filter: 3\n"
      "  default_encoder:\n" +
      EveryOptionYaml(encoder::OptionStage::kEncoder) +
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
      "    preprocess:\n"
      "      noise_gate_db: 30\n"
      "      new_filter: 2\n"
      "    encoder:\n"
      "      gate_db: 40\n"
      "      new_thing: \"on\"\n"
      "    rating: 5\n"
      "exports:\n"
      "  - name: \"e\"\n"
      "    format: \"acorn-phrom\"\n"
      "    compress: \"MIT\"\n";
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
  EXPECT_EQ(phrase.preprocess_unknown, (UnknownKeys{{"new_filter", "2"}}));
  EXPECT_EQ(phrase.encoder.at("gate_db"), "40");
  EXPECT_EQ(phrase.encoder.at("noise_gate_db"), "30");
  EXPECT_EQ(loaded->Settings().default_encoder_unknown,
            (UnknownKeys{{"future_option", "7"}}));
  EXPECT_EQ(loaded->Settings().default_preprocess_unknown,
            (UnknownKeys{{"future_filter", "3"}}));
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
  EXPECT_THAT(yaml, HasSubstr("    de_ess_db: null\n    future_filter: 3\n"));
  EXPECT_THAT(yaml, HasSubstr("    preprocess:\n      noise_gate_db: 30\n"
                              "      new_filter: 2\n    encoder:\n"));
  EXPECT_THAT(yaml, HasSubstr("      colour: \"NO\"\n"));
  EXPECT_THAT(yaml, HasSubstr("      bits: 24\n"));
  EXPECT_THAT(yaml, HasSubstr("      new_thing: \"on\"\n"));
  EXPECT_THAT(yaml, HasSubstr("    rating: 5\n"));
  EXPECT_THAT(yaml, HasSubstr("    compress: \"MIT\"\n"));
  const std::vector<std::uint8_t>& frames_bytes =
      saved.at(PhraseFile(1, "frames.yaml"));
  const std::string frames(frames_bytes.begin(), frames_bytes.end());
  EXPECT_THAT(frames, HasSubstr("  stoi: 0.9\n"));
  // PESQ, written before the quality measures, is dropped.
  EXPECT_THAT(frames, Not(HasSubstr("pesq")));
  EXPECT_FALSE(phrase.encoding->quality.has_value());
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

TEST_F(ProjectSerializerTest, LoadsAProjectListingEveryOption) {
  const StatusOr<Project> loaded = Load(WithProjectYaml(MinimalProjectYaml()));
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(*loaded, Project());
}

// A project lists every encoder option, so changing a default never
// changes it; one that does not, such as one saved with the squelch the
// noise gate replaced, or before the de-esser, is refused.
TEST_F(ProjectSerializerTest, RefusesAProjectThatDoesNotListEveryOption) {
  const std::string head = "format: \"speak5220-project\"\nversion: 1\n";
  const StatusOr<Project> bare = Load(WithProjectYaml(head));
  ASSERT_FALSE(bare.IsOk());
  EXPECT_EQ(bare.GetStatus().Code(), StatusCode::kDataLoss);
  EXPECT_THAT(bare.GetStatus().Message(),
              HasSubstr("project.default_preprocess is missing"));

  // Each mapping is checked for its own options.
  const std::vector<std::pair<std::vector<std::string>, std::string>> cases = {
      {{"    noise_gate_db: none\n", "    treble_tilt: 0\n"},
       "project.default_preprocess does not list noise_gate_db, treble_tilt"},
      {{"    noise_gate_db: none\n", "    noise_gate_ms: 100\n"},
       "project.default_preprocess does not list noise_gate_db, "
       "noise_gate_ms"},
      {{"    treble_tilt: 0\n", "    de_ess_db: none\n"},
       "project.default_preprocess does not list treble_tilt, de_ess_db"},
      {{"    resonance_hold_db: 14\n", "    voicing_min_run: 2\n"},
       "project.default_encoder does not list resonance_hold_db, "
       "voicing_min_run"},
  };
  for (const auto& [lines, message] : cases) {
    std::string older = MinimalProjectYaml();
    for (const std::string& line : lines) {
      ASSERT_NE(older.find(line), std::string::npos) << line;
      older.erase(older.find(line), line.size());
    }
    const StatusOr<Project> loaded = Load(WithProjectYaml(older));
    ASSERT_FALSE(loaded.IsOk());
    EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr(message));
    EXPECT_THAT(loaded.GetStatus().Message(), HasSubstr("cannot be opened"));
  }
}

// An option belongs in its own stage's mapping: a preprocessing option
// under default_encoder, or an encoder option under a phrase's
// preprocess, is refused, naming where it belongs.
TEST_F(ProjectSerializerTest, RefusesAnOptionInTheOtherStagesMapping) {
  const StatusOr<Project> defaults =
      Load(WithProjectYaml(MinimalProjectYaml() + "    bass_cut_hz: 200\n"));
  ASSERT_FALSE(defaults.IsOk());
  EXPECT_THAT(defaults.GetStatus().Message(),
              HasSubstr("project.default_encoder.bass_cut_hz belongs under "
                        "default_preprocess"));
  const StatusOr<Project> phrase = Load(
      WithProjectYaml(MinimalProjectYaml() + "phrases:\n" +
                      PhraseYaml(1, "    preprocess:\n      gate_db: 40\n")));
  ASSERT_FALSE(phrase.IsOk());
  EXPECT_THAT(phrase.GetStatus().Message(),
              HasSubstr("gate_db belongs under encoder"));
}

// Options the encoder now fixes (encoder_options_test.cpp) are no longer
// options: a project that lists them opens, and keeps them as it keeps any
// key it does not know.
TEST_F(ProjectSerializerTest, OpensAProjectThatListsAFixedOption) {
  const std::string yaml =
      MinimalProjectYaml() +
      "    quiet_pauses: true\n    compression: 1\n"
      "phrases:\n" +
      PhraseYaml(1, "    encoder:\n      repeats: false\n");
  const StatusOr<Project> loaded = Load(WithProjectYaml(yaml));
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_TRUE(loaded->Phrases().front().encoder.empty());
  const std::string saved = ProjectYamlOf(*loaded);
  EXPECT_THAT(saved, HasSubstr("    quiet_pauses: true\n    compression: 1\n"));
  EXPECT_THAT(saved, HasSubstr("      repeats: false\n"));
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
      {head + "project:\n  default_encoder:\n    window: 240.5\n",
       "window must be a whole number"},
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
      MinimalProjectYaml() + "phrases:\n  - id: \"" + TestId(1) +
      "\"\n    name: \"A\"\n    source: {file: \"a.wav\"}\n";
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
