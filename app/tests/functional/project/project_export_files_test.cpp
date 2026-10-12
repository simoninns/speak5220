/*
 * File:        project_export_files_test.cpp
 * Module:      project tests
 * Purpose:     Checks exported files with real tools: the C headers compile
 *              as C and C++, and the WAV files reload at 8 kHz with the
 *              expected length
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/lpc/frame_codec.h"
#include "core/project/directory_project_store.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/project/project_serializer.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::project {
namespace {

namespace fs = std::filesystem;

// Time allowed for one compiler run.
constexpr int kCompileTimeoutMs = 120000;

// Includes both headers, checks their sizes at compile time and uses every
// table, so the compiler sees all of it.
constexpr std::string_view kUseHeaders = R"(#include "phrom.h"
#include "words.h"
#include "phrom.h"

#ifdef __cplusplus
#define CHECK(condition) static_assert(condition, #condition)
#else
#define CHECK(condition) _Static_assert(condition, #condition)
#endif

CHECK(sizeof(phrom_phrom_1) == PHROM_PHROM_SIZE);
CHECK(sizeof(phrom_words) / sizeof(phrom_words[0]) == PHROM_WORDS);
CHECK(sizeof(words_words) / sizeof(words_words[0]) == WORDS_WORDS);
CHECK(sizeof(words_word_one_won) == WORDS_LENGTH_ONE_WON);

int first_byte(void) {
  return phrom_phrom_1[0] + phrom_words[PHROM_WORD_W_2_TWEN].number +
         words_words[WORDS_WORD_ING].data[0];
}
)";

class ProjectExportFilesTest : public ::testing::Test {
 protected:
  void SetUp() override {
    ASSERT_TRUE(directory_.isValid());
    DirectoryProjectStore store(fs::path(SPEAK5220_PROJECTS_DIR) /
                                "general/many-phrases");
    StatusOr<Project> loaded = serializer_.Load(store);
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
    project_ = *std::move(loaded);
  }

  fs::path Path(const std::string& name) const {
    return fs::path(directory_.path().toStdString()) / name;
  }

  // Runs `preset` and writes its files into the temporary directory.
  std::vector<ExportedFile> Export(const ExportPreset& preset,
                                   std::string_view base) {
    StatusOr<std::vector<ExportedFile>> files = RunExport(
        project_, preset, base, {.synth = &synth_, .wav_codec = &codec_});
    EXPECT_TRUE(files.IsOk()) << files.GetStatus().ToString();
    if (!files.IsOk()) {
      return {};
    }
    EXPECT_TRUE(
        WriteExportedFiles(directory_.path().toStdString(), *files).IsOk());
    return *std::move(files);
  }

  // Compiles kUseHeaders as `language` ("c" or "c++") with the compiler
  // that built the tests, every warning an error; the test fails, with the
  // compiler's messages, if it does not compile.
  void Compile(std::string_view language) {
    const bool c = language == "c";
    const fs::path source = Path(c ? "use.c" : "use.cpp");
    std::ofstream(source) << kUseHeaders;
    const QString compiler = QStringLiteral(SPEAK5220_CXX_COMPILER);
    QStringList arguments;
    if (QStringLiteral(SPEAK5220_CXX_COMPILER_ID) == QStringLiteral("MSVC")) {
      arguments << QStringLiteral("/nologo") << QStringLiteral("/W4")
                << QStringLiteral("/WX") << QStringLiteral("/c")
                << QString::fromLatin1(c ? "/TC" : "/TP")
                << QString::fromLatin1(c ? "/std:c11" : "/std:c++17")
                << QStringLiteral("/Fo%1").arg(
                       QString::fromStdString(Path("use.obj").string()));
    } else {
      arguments << QStringLiteral("-x") << QString::fromUtf8(language.data())
                << QString::fromLatin1(c ? "-std=c11" : "-std=c++17")
                << QStringLiteral("-Wall") << QStringLiteral("-Wextra")
                << QStringLiteral("-Wpedantic") << QStringLiteral("-Werror")
                << QStringLiteral("-c") << QStringLiteral("-o")
                << QString::fromStdString(Path("use.o").string());
    }
    arguments << QString::fromStdString(source.string());
    QProcess process;
    process.setWorkingDirectory(directory_.path());
    process.setProcessChannelMode(QProcess::MergedChannels);
    process.start(compiler, arguments);
    ASSERT_TRUE(process.waitForFinished(kCompileTimeoutMs))
        << "the compiler did not finish";
    EXPECT_EQ(process.exitCode(), 0)
        << language << ": "
        << QString::fromUtf8(process.readAll()).toStdString();
  }

  QTemporaryDir directory_;
  audio::SndfileWavCodec codec_;
  synth::Synth synth_;
  ProjectSerializer serializer_{codec_};
  Project project_;
};

TEST_F(ProjectExportFilesTest, HeadersCompileAsCAndCpp) {
  ASSERT_EQ(Export({.name = "h", .format = "c-header-phrom"}, "phrom").size(),
            1U);
  ASSERT_EQ(Export({.name = "h", .format = "c-header-speak-external"}, "words")
                .size(),
            1U);
  Compile("c");
  Compile("c++");
}

TEST_F(ProjectExportFilesTest, EncodedWavReloadsAt8kHz) {
  const std::vector<ExportedFile> files =
      Export({.name = "w", .format = "wav"}, "chip");
  ASSERT_EQ(files.size(), project_.Phrases().size());
  audio::SndfileWavReader reader;
  for (std::size_t i = 0; i < files.size(); ++i) {
    const Phrase& phrase = project_.Phrases()[i];
    const StatusOr<std::vector<std::uint8_t>> packed =
        lpc::FrameCodec::Pack(phrase.encoding->frames);
    ASSERT_TRUE(packed.IsOk());
    const StatusOr<synth::SynthResult> spoken =
        synth_.Synthesize(*packed, synth::SynthPath::kVsm);
    ASSERT_TRUE(spoken.IsOk());
    const StatusOr<audio::DecodedWav> wav = reader.Read(Path(files[i].name));
    ASSERT_TRUE(wav.IsOk()) << files[i].name;
    EXPECT_EQ(wav->sample_rate, kProjectSampleRate);
    EXPECT_EQ(wav->format, audio::WavSampleFormat::kPcm16);
    EXPECT_EQ(wav->samples.size(), spoken->samples.size()) << phrase.name;
  }
}

TEST_F(ProjectExportFilesTest, OriginalWavReloadsAsTheSource) {
  const std::vector<ExportedFile> files = Export({.name = "w",
                                                  .format = "wav",
                                                  .scope = "selected",
                                                  .phrases = {"SEVEN"},
                                                  .audio = "original"},
                                                 "source");
  ASSERT_EQ(files.size(), 1U);
  EXPECT_EQ(files[0].name, "source_SEVEN.wav");
  const Phrase* seven = nullptr;
  for (const Phrase& phrase : project_.Phrases()) {
    if (phrase.name == "SEVEN") {
      seven = &phrase;
    }
  }
  ASSERT_NE(seven, nullptr);
  const StatusOr<audio::DecodedWav> wav =
      audio::SndfileWavReader().Read(Path(files[0].name));
  ASSERT_TRUE(wav.IsOk());
  EXPECT_EQ(wav->sample_rate, kProjectSampleRate);
  EXPECT_EQ(wav->format, audio::WavSampleFormat::kPcm24);
  EXPECT_EQ(wav->samples, seven->audio);
}

}  // namespace
}  // namespace speak5220::core::project
