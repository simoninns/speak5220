/*
 * File:        cli_project_test.cpp
 * Module:      cli tests
 * Purpose:     Runs speak5220-cli's project commands end to end: new, add,
 *              encode and export
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/audio/wav_io.h"
#include "core/encoder/encoder.h"
#include "core/project/encoding_cache.h"
#include "core/project/project.h"
#include "core/project/project_serializer.h"
#include "core/project/project_store.h"
#include "core/project/zip_project_store.h"
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
  ASSERT_TRUE(loaded
                  ->SetExports({{.name = "phrom", .format = "acorn-phrom"},
                                {.name = "joined",
                                 .format = "acorn-phrom",
                                 .layout = "concatenated"}})
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

  run = RunCli({QStringLiteral("export"), Q(path), QStringLiteral("--output"),
                Q(Path("one")), QStringLiteral("--preset"),
                QStringLiteral("joined")});
  ASSERT_EQ(run.exit_code, 0) << run.err;
  EXPECT_TRUE(fs::exists(Path("one") / "joined.bin"));
  EXPECT_FALSE(fs::exists(Path("one") / "phrom_1.bin"));
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

TEST_F(CliProjectTest, UnusableCommandLinesExitWithCode2) {
  EXPECT_EQ(RunCli({QStringLiteral("frobnicate")}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("new")}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("add"), Q(Path("p"))}).exit_code, 2);
  EXPECT_EQ(RunCli({QStringLiteral("export"), Q(Path("p"))}).exit_code, 2);
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
