/*
 * File:        cli_project_test.cpp
 * Module:      cli tests
 * Purpose:     Runs speak5220-cli's project commands end to end: new, add,
 *              import-phrom, encode, export, synth and measure
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/audio/wav_io.h"
#include "core/encoder/encoder.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/project/encoding_cache.h"
#include "core/project/project.h"
#include "core/project/project_serializer.h"
#include "core/project/project_store.h"
#include "core/project/zip_project_store.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/synthetic_speech.h"

namespace speak5220::cli {
namespace {

namespace fs = std::filesystem;
namespace project = core::project;
using ::testing::HasSubstr;

// Time allowed for one command, encoding included.
constexpr int kProcessTimeoutMs = 120000;

struct CliRun {
  int exit_code = -1;
  std::string out;
  std::string err;
};

CliRun RunCli(const QStringList& arguments) {
  QProcess process;
  process.start(QStringLiteral(SPEAK5220_CLI_PATH), arguments);
  EXPECT_TRUE(process.waitForFinished(kProcessTimeoutMs))
      << "speak5220-cli did not finish";
  EXPECT_EQ(process.exitStatus(), QProcess::NormalExit);
  return {
      .exit_code = process.exitCode(),
      .out = QString::fromUtf8(process.readAllStandardOutput()).toStdString(),
      .err = QString::fromUtf8(process.readAllStandardError()).toStdString()};
}

QString Q(const fs::path& path) {
  return QString::fromStdString(path.string());
}

std::vector<char> ReadAll(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

class CliProjectTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(directory_.isValid());
    // A second of speech-like sound at 8 kHz.
    const std::vector<double> speech = testing::SyntheticSpeech();
    std::vector<float> samples(speech.begin(), speech.end());
    ASSERT_TRUE(core::audio::SndfileWavWriter()
                    .Write(Path("speech.wav"), samples, 8000,
                           core::audio::WavOutputFormat::kPcm16)
                    .IsOk());
  }

  fs::path Path(const std::string& name) const {
    return fs::path(directory_.path().toStdString()) / name;
  }

  core::StatusOr<project::Project> Load(const fs::path& path) {
    return serializer_.Load(*project::MakeProjectStore(path));
  }

  // Creates PATH and adds speech.wav as HELLO, checking each step.
  void CreateWithPhrase(const fs::path& path) {
    CliRun run = RunCli({QStringLiteral("new"), Q(path),
                         QStringLiteral("--name"), QStringLiteral("Test")});
    ASSERT_EQ(run.exit_code, 0) << run.err;
    run = RunCli({QStringLiteral("add"), Q(path), Q(Path("speech.wav")),
                  QStringLiteral("--name"), QStringLiteral("hello")});
    ASSERT_EQ(run.exit_code, 0) << run.err;
    EXPECT_THAT(run.out, HasSubstr("added HELLO from speech.wav"));
  }

  QTemporaryDir directory_;
  core::audio::SndfileWavCodec codec_;
  project::ProjectSerializer serializer_{codec_};
};

TEST_F(CliProjectTest, CreatesAddsEncodesSavesAndReloadsIdentically) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  const CliRun run = RunCli({QStringLiteral("encode"), Q(path)});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_THAT(run.out, HasSubstr("encoded 1 phrase(s); 1 PHROM(s) of 64"));

  const core::StatusOr<project::Project> loaded = Load(path);
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(loaded->Settings().name, "Test");
  ASSERT_EQ(loaded->Phrases().size(), 1U);
  const project::Phrase& phrase = loaded->Phrases()[0];
  EXPECT_EQ(phrase.name, "HELLO");
  ASSERT_TRUE(phrase.encoding.has_value());
  EXPECT_GT(phrase.encoding->frames.size(), 10U);
  EXPECT_TRUE(*project::EncodingCache::IsCurrent(
      phrase, loaded->Settings().default_encoder));

  // The encoding is the encoder's own for the stored audio.
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  const std::vector<double> samples(phrase.audio.begin(), phrase.audio.end());
  const auto encoded =
      encoder->Encode(samples, 8000, core::encoder::EncoderOptions{});
  ASSERT_TRUE(encoded.IsOk());
  EXPECT_EQ(phrase.encoding->frames, encoded->frames);

  // Saved again in the other form, it reloads to the same project.
  ASSERT_TRUE(
      serializer_.Save(*loaded, *project::MakeProjectStore(Path("copy")))
          .IsOk());
  const core::StatusOr<project::Project> reloaded = Load(Path("copy"));
  ASSERT_TRUE(reloaded.IsOk()) << reloaded.GetStatus().ToString();
  EXPECT_EQ(*reloaded, *loaded);

  // Encoding again has nothing to do and leaves the file byte-identical.
  const std::vector<char> before = ReadAll(path);
  const CliRun again = RunCli({QStringLiteral("encode"), Q(path)});
  ASSERT_EQ(again.exit_code, 0) << again.err;
  EXPECT_THAT(again.out, HasSubstr("encoded 0 phrase(s)"));
  EXPECT_EQ(ReadAll(path), before);
}

TEST_F(CliProjectTest, OutputDirectoryLeavesTheProjectUntouched) {
  const fs::path path = Path("words");
  CreateWithPhrase(path);
  const std::vector<char> before = ReadAll(path / "project.yaml");
  const CliRun run = RunCli({QStringLiteral("encode"), Q(path),
                             QStringLiteral("--output"), Q(Path("out"))});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_EQ(ReadAll(path / "project.yaml"), before);
  const core::StatusOr<project::Project> original = Load(path);
  const core::StatusOr<project::Project> encoded = Load(Path("out") / "words");
  ASSERT_TRUE(original.IsOk());
  ASSERT_TRUE(encoded.IsOk()) << encoded.GetStatus().ToString();
  EXPECT_FALSE(original->Phrases()[0].encoding.has_value());
  EXPECT_TRUE(encoded->Phrases()[0].encoding.has_value());
}

TEST_F(CliProjectTest, ExportsPhromImages) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  ASSERT_EQ(RunCli({QStringLiteral("encode"), Q(path)}).exit_code, 0);
  const CliRun run =
      RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
              Q(Path("export")), QStringLiteral("--format"),
              QStringLiteral("acorn-phrom")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_EQ(fs::file_size(Path("export") / "test_1.bin"), 16384U);
}

TEST_F(CliProjectTest, ExportRunsTheProjectsPresets) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  ASSERT_EQ(RunCli({QStringLiteral("encode"), Q(path)}).exit_code, 0);
  core::StatusOr<project::Project> loaded = Load(path);
  ASSERT_TRUE(loaded.IsOk());
  ASSERT_TRUE(
      loaded
          ->SetExports(
              {{.name = "phrom", .format = "acorn-phrom"},
               {.name = "joined",
                .format = "acorn-phrom",
                .layout = "concatenated"},
               {.name = "header", .format = "c-header-phrom", .licence = "MIT"},
               {.name = "chip", .format = "wav"}})
          .IsOk());
  ASSERT_TRUE(
      serializer_.Save(*loaded, *project::MakeProjectStore(path)).IsOk());

  CliRun run = RunCli({QStringLiteral("export"), Q(path),
                       QStringLiteral("--output"), Q(Path("all"))});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_TRUE(fs::exists(Path("all") / "phrom_1.bin"));
  EXPECT_TRUE(fs::exists(Path("all") / "joined.bin"));
  EXPECT_EQ(ReadAll(Path("all") / "phrom_1.bin"),
            ReadAll(Path("all") / "joined.bin"));
  const std::vector<char> header = ReadAll(Path("all") / "header.h");
  EXPECT_THAT(std::string(header.begin(), header.end()),
              HasSubstr("SPDX-License-Identifier: MIT\n"));
  EXPECT_TRUE(fs::exists(Path("all") / "chip_HELLO.wav"));

  run = RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
                Q(Path("one")), QStringLiteral("--preset"),
                QStringLiteral("joined")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_TRUE(fs::exists(Path("one") / "joined.bin"));
  EXPECT_FALSE(fs::exists(Path("one") / "phrom_1.bin"));
}

TEST_F(CliProjectTest, ExportsEachFormatFromTheCommandLine) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  ASSERT_EQ(RunCli({QStringLiteral("encode"), Q(path)}).exit_code, 0);
  CliRun run =
      RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
              Q(Path("h")), QStringLiteral("--format"),
              QStringLiteral("c-header-speak-external"),
              QStringLiteral("--phrase"), QStringLiteral("HELLO"),
              QStringLiteral("--licence"), QStringLiteral("CC0-1.0")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  const std::vector<char> header = ReadAll(Path("h") / "test.h");
  const std::string text(header.begin(), header.end());
  EXPECT_THAT(text, HasSubstr("SPDX-License-Identifier: CC0-1.0\n"));
  EXPECT_THAT(text, HasSubstr("TEST_LENGTH_HELLO"));

  run = RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
                Q(Path("w")), QStringLiteral("--format"), QStringLiteral("wav"),
                QStringLiteral("--audio"), QStringLiteral("original")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  const core::StatusOr<core::audio::DecodedWav> wav =
      core::audio::SndfileWavReader().Read(Path("w") / "test_HELLO.wav");
  ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
  EXPECT_EQ(wav->sample_rate, 8000);
}

TEST_F(CliProjectTest, ExportRefusesUnknownPhrasesAndBadPresets) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  ASSERT_EQ(RunCli({QStringLiteral("encode"), Q(path)}).exit_code, 0);
  CliRun run =
      RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
              Q(Path("x")), QStringLiteral("--format"), QStringLiteral("wav"),
              QStringLiteral("--phrase"), QStringLiteral("NOPE")});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("NOPE"));
  run = RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
                Q(Path("x")), QStringLiteral("--format"), QStringLiteral("wav"),
                QStringLiteral("--layout"), QStringLiteral("concatenated")});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("layout"));

  core::StatusOr<project::Project> loaded = Load(path);
  ASSERT_TRUE(loaded.IsOk());
  ASSERT_TRUE(
      loaded->SetExports({{.name = "../up", .format = "acorn-phrom"}}).IsOk());
  ASSERT_TRUE(
      serializer_.Save(*loaded, *project::MakeProjectStore(path)).IsOk());
  run = RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
                Q(Path("x"))});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_FALSE(fs::exists(Path("up_1.bin")));
}

TEST_F(CliProjectTest, ExportRefusesPhrasesThatAreNotEncoded) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  const CliRun run =
      RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
              Q(Path("export")), QStringLiteral("--format"),
              QStringLiteral("acorn-phrom")});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("HELLO is not encoded"));
}

TEST_F(CliProjectTest, AddSetsTheDescriptionOfOnePhrase) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  const CliRun run =
      RunCli({QStringLiteral("add"), Q(path), Q(Path("speech.wav")),
              QStringLiteral("--name"), QStringLiteral("AGAIN"),
              QStringLiteral("--description"),
              QStringLiteral("Lord, but I'm glad to see you again, Phil.")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  const core::StatusOr<project::Project> loaded = Load(path);
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  ASSERT_EQ(loaded->Phrases().size(), 2U);
  EXPECT_EQ(loaded->Phrases()[0].description, "");
  EXPECT_EQ(loaded->Phrases()[1].description,
            "Lord, but I'm glad to see you again, Phil.");
}

TEST_F(CliProjectTest, NewSetsTheDefaultEncoderOptions) {
  const fs::path path = Path("options.s5proj");
  const CliRun run =
      RunCli({QStringLiteral("new"), Q(path), QStringLiteral("--option"),
              QStringLiteral("repeat_lambda=300"), QStringLiteral("--option"),
              QStringLiteral("bass_cut_hz=200")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  const core::StatusOr<project::Project> loaded = Load(path);
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(loaded->Settings().default_encoder.repeat_lambda, 300.0);
  EXPECT_EQ(loaded->Settings().default_encoder.preprocess.bass_cut_hz, 200.0);

  CliRun bad =
      RunCli({QStringLiteral("new"), Q(Path("bad.s5proj")),
              QStringLiteral("--option"), QStringLiteral("no_such_option=1")});
  EXPECT_EQ(bad.exit_code, 1);
  EXPECT_FALSE(fs::exists(Path("bad.s5proj")));
  bad = RunCli({QStringLiteral("new"), Q(Path("bad.s5proj")),
                QStringLiteral("--option"), QStringLiteral("repeat_lambda")});
  EXPECT_EQ(bad.exit_code, 2);
}

TEST_F(CliProjectTest, SynthWritesTheChipsOutputOfAFramesFile) {
  const std::vector<core::lpc::Frame> frames = {
      core::lpc::Frame::Voiced(10, 30, {20, 10, 7, 8, 8, 8, 8, 4, 4, 4}),
      core::lpc::Frame::Repeat(9, 30),
      core::lpc::Frame::Unvoiced(6, {20, 10, 7, 8}), core::lpc::Frame::Stop()};
  const fs::path yaml = Path("word.frames.yaml");
  std::ofstream(yaml) << project::FramesYaml({.frames = frames});
  const CliRun run = RunCli({QStringLiteral("synth"), Q(yaml),
                             QStringLiteral("--output"), Q(Path("word.wav"))});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_THAT(run.out, HasSubstr("4 frames"));

  const core::StatusOr<core::audio::DecodedWav> wav =
      core::audio::SndfileWavReader().Read(Path("word.wav"));
  ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
  EXPECT_EQ(wav->sample_rate, 8000);
  const core::StatusOr<std::vector<std::uint8_t>> packed =
      core::lpc::FrameCodec::Pack(frames);
  ASSERT_TRUE(packed.IsOk());
  core::synth::Synth synth;
  const core::StatusOr<core::synth::SynthResult> spoken =
      synth.Synthesize(*packed, core::synth::SynthPath::kVsm);
  ASSERT_TRUE(spoken.IsOk());
  ASSERT_EQ(wav->samples.size(), spoken->samples.size());
  for (std::size_t i = 0; i < wav->samples.size(); ++i) {
    ASSERT_EQ(static_cast<int>(wav->samples[i] * 32768.0F), spoken->samples[i])
        << "sample " << i;
  }

  EXPECT_EQ(RunCli({QStringLiteral("synth"), Q(yaml)}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("synth"), Q(Path("missing.yaml")),
                    QStringLiteral("--output"), Q(Path("x.wav"))})
                .exit_code,
            1);
}

TEST_F(CliProjectTest, MeasureWritesOneRowPerPhrase) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  CliRun run = RunCli({QStringLiteral("measure"), Q(path),
                       QStringLiteral("--output"), Q(Path("measures"))});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("has no encoding"));

  ASSERT_EQ(RunCli({QStringLiteral("encode"), Q(path)}).exit_code, 0);
  run = RunCli({QStringLiteral("measure"), Q(path), QStringLiteral("--output"),
                Q(Path("measures"))});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  std::ifstream tsv(Path("measures") / "measures.tsv");
  std::string header;
  std::string row;
  ASSERT_TRUE(std::getline(tsv, header));
  ASSERT_TRUE(std::getline(tsv, row));
  EXPECT_THAT(header, HasSubstr("score"));
  EXPECT_THAT(header, HasSubstr("a_weighted_rms"));
  const auto columns = [](const std::string& line) {
    std::vector<std::string> fields;
    std::stringstream stream(line);
    std::string field;
    while (std::getline(stream, field, '\t')) {
      fields.push_back(field);
    }
    return fields;
  };
  const std::vector<std::string> names = columns(header);
  const std::vector<std::string> values = columns(row);
  ASSERT_EQ(values.size(), names.size());
  EXPECT_EQ(values[0], "HELLO");
  EXPECT_EQ(values[1], "speech.wav");
  for (std::size_t i = 2; i < values.size(); ++i) {
    EXPECT_FALSE(values[i].empty()) << names[i];
  }
  std::string extra;
  EXPECT_FALSE(std::getline(tsv, extra));
}

TEST_F(CliProjectTest, FailuresExitWithCode1) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  CliRun run = RunCli({QStringLiteral("add"), Q(path), Q(Path("speech.wav")),
                       QStringLiteral("--name"), QStringLiteral("HELLO")});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("already a phrase called HELLO"));
  run = RunCli({QStringLiteral("add"), Q(path), Q(Path("speech.wav")),
                QStringLiteral("--name"), QStringLiteral("HELLO!")});
  EXPECT_EQ(run.exit_code, 1);
  run = RunCli({QStringLiteral("new"), Q(path)});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("already exists"));
  run = RunCli({QStringLiteral("encode"), Q(Path("missing.s5proj"))});
  EXPECT_EQ(run.exit_code, 1);
}

TEST_F(CliProjectTest, ImportPhromAddsEachImagesWordsAsLockedPhrases) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  ASSERT_EQ(RunCli({QStringLiteral("encode"), Q(path)}).exit_code, 0);
  CliRun run =
      RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
              Q(Path("export")), QStringLiteral("--format"),
              QStringLiteral("acorn-phrom")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  const fs::path image = Path("export") / "test_1.bin";

  // Into --output: the project itself is left as it was.
  const std::vector<char> before = ReadAll(path);
  run = RunCli({QStringLiteral("import-phrom"), Q(path), Q(image), Q(image),
                QStringLiteral("--output"), Q(Path("out"))});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_THAT(run.out, HasSubstr("added HELLO 2 from test_1.bin"));
  EXPECT_THAT(run.out, HasSubstr("added HELLO 3 from test_1.bin"));
  EXPECT_THAT(run.out, HasSubstr("imported 2 word(s) as locked phrases"));
  EXPECT_EQ(ReadAll(path), before);

  const core::StatusOr<project::Project> loaded =
      Load(Path("out") / "test.s5proj");
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  ASSERT_EQ(loaded->Phrases().size(), 3U);
  const project::Phrase& original = loaded->Phrases()[0];
  for (std::size_t i = 1; i < 3; ++i) {
    const project::Phrase& imported = loaded->Phrases()[i];
    EXPECT_TRUE(imported.locked);
    EXPECT_FALSE(imported.source.has_value());
    EXPECT_TRUE(imported.audio.empty());
    ASSERT_TRUE(imported.encoding.has_value());
    EXPECT_EQ(imported.encoding->frames, original.encoding->frames);
  }

  // In place, and the encode command leaves the imported words alone.
  run = RunCli({QStringLiteral("import-phrom"), Q(path), Q(image)});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  run = RunCli({QStringLiteral("encode"), Q(path), QStringLiteral("--force")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_THAT(run.out, HasSubstr("encoded 1 phrase(s)"));
  const core::StatusOr<project::Project> in_place = Load(path);
  ASSERT_TRUE(in_place.IsOk()) << in_place.GetStatus().ToString();
  ASSERT_EQ(in_place->Phrases().size(), 2U);
  EXPECT_EQ(in_place->Phrases()[1].name, "HELLO 2");
  EXPECT_TRUE(in_place->Phrases()[1].locked);
}

TEST_F(CliProjectTest, ImportPhromRefusesFilesThatAreNotPhromImages) {
  const fs::path path = Path("test.s5proj");
  CreateWithPhrase(path);
  const std::vector<char> before = ReadAll(path);
  {
    std::ofstream(Path("short.bin"), std::ios::binary) << "not a PHROM";
    std::ofstream zeros(Path("zeros.bin"), std::ios::binary);
    const std::string bank(16384, '\0');
    zeros << bank;
  }
  CliRun run =
      RunCli({QStringLiteral("import-phrom"), Q(path), Q(Path("short.bin"))});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("multiple of 16384"));
  run = RunCli({QStringLiteral("import-phrom"), Q(path), Q(Path("zeros.bin"))});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("not an Acorn PHROM image"));
  run =
      RunCli({QStringLiteral("import-phrom"), Q(path), Q(Path("missing.bin"))});
  EXPECT_EQ(run.exit_code, 1);
  EXPECT_THAT(run.err, HasSubstr("cannot open"));
  EXPECT_EQ(ReadAll(path), before);
}

TEST_F(CliProjectTest, UnusableCommandLinesExitWithCode2) {
  EXPECT_EQ(RunCli({QStringLiteral("import-phrom"), Q(Path("p"))}).exit_code,
            2);
  EXPECT_EQ(RunCli({QStringLiteral("frobnicate")}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("new")}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("add"), Q(Path("p"))}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("export"), Q(Path("p"))}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("export"), Q(Path("p")),
                    QStringLiteral("--output"), Q(Path("o")),
                    QStringLiteral("--licence"), QStringLiteral("MIT")})
                .exit_code,
            2);
  EXPECT_EQ(RunCli({QStringLiteral("add"), Q(Path("p")),
                    QStringLiteral("a.wav"), QStringLiteral("b.wav"),
                    QStringLiteral("--name"), QStringLiteral("X")})
                .exit_code,
            2);
  EXPECT_EQ(RunCli({QStringLiteral("add"), Q(Path("p")),
                    QStringLiteral("a.wav"), QStringLiteral("b.wav"),
                    QStringLiteral("--description"), QStringLiteral("X")})
                .exit_code,
            2);
}

}  // namespace
}  // namespace speak5220::cli
