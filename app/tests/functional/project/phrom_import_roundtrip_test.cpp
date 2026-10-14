/*
 * File:        phrom_import_roundtrip_test.cpp
 * Module:      project tests
 * Purpose:     Exports fixture projects' Acorn PHROM images, imports them
 *              back as locked phrases, and checks the frames, the
 *              re-exported bytes and that nothing is re-encoded
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QTemporaryDir>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/lpc/frame.h"
#include "core/project/directory_project_store.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/phrom_import.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/project/project_serializer.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/project_mocks.h"

namespace speak5220::core::project {
namespace {

namespace fs = std::filesystem;
using ::testing::StrictMock;

// The phrases ImportPhromImage() makes of `bytes`.
StatusOr<std::vector<Phrase>> ImportWords(std::span<const std::uint8_t> bytes,
                                          const std::set<std::string>& taken) {
  StatusOr<PhromImport> imported = ImportPhromImage(bytes, taken);
  if (!imported.IsOk()) {
    return imported.GetStatus();
  }
  return std::move(imported->phrases);
}

std::vector<std::uint8_t> ReadBytes(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  EXPECT_TRUE(in) << path;
  return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

ExportPreset PhromPreset(std::string_view layout) {
  return {.name = "phrom",
          .format = "acorn-phrom",
          .scope = std::string(kScopeAll),
          .layout = std::string(layout)};
}

class PhromImportRoundTripTest : public ::testing::TestWithParam<const char*> {
 protected:
  void SetUp() override {
    ASSERT_TRUE(directory_.isValid());
    DirectoryProjectStore store(fs::path(SPEAK5220_PROJECTS_DIR) / GetParam());
    StatusOr<Project> loaded = serializer_.Load(store);
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
    original_ = *std::move(loaded);
  }

  // Runs `preset` on `project`, writes the files into `subdirectory` of
  // the temporary directory, and reads each back from disk.
  std::vector<std::vector<std::uint8_t>> Export(
      const Project& project, const ExportPreset& preset,
      const std::string& subdirectory) {
    StatusOr<std::vector<ExportedFile>> files =
        RunExport(project, preset, "image", {});
    EXPECT_TRUE(files.IsOk()) << files.GetStatus().ToString();
    if (!files.IsOk()) {
      return {};
    }
    const fs::path where =
        fs::path(directory_.path().toStdString()) / subdirectory;
    EXPECT_TRUE(WriteExportedFiles(where, *files).IsOk());
    std::vector<std::vector<std::uint8_t>> images;
    for (const ExportedFile& file : *files) {
      images.push_back(ReadBytes(where / file.name));
      EXPECT_EQ(images.back(), file.bytes);
    }
    return images;
  }

  // A project with the original's settings and the imported phrases.
  Project ImportedProject(const std::vector<std::uint8_t>& bytes) {
    StatusOr<std::vector<Phrase>> phrases = ImportWords(bytes, {});
    EXPECT_TRUE(phrases.IsOk()) << phrases.GetStatus().ToString();
    ProjectContents contents;
    contents.settings = original_.Settings();
    if (phrases.IsOk()) {
      contents.phrases = *std::move(phrases);
    }
    StatusOr<Project> project = Project::FromContents(std::move(contents));
    EXPECT_TRUE(project.IsOk()) << project.GetStatus().ToString();
    return project.IsOk() ? *std::move(project) : Project();
  }

  QTemporaryDir directory_;
  audio::SndfileWavCodec codec_;
  ProjectSerializer serializer_{codec_};
  Project original_;
};

TEST_P(PhromImportRoundTripTest,
       ImportsTheExportedFramesAndReExportsTheSameBytes) {
  const std::vector<std::vector<std::uint8_t>> exported =
      Export(original_, PhromPreset(kLayoutConcatenated), "exported");
  ASSERT_EQ(exported.size(), 1U);
  const Project imported = ImportedProject(exported[0]);

  ASSERT_EQ(imported.Phrases().size(), original_.Phrases().size());
  for (std::size_t i = 0; i < imported.Phrases().size(); ++i) {
    const Phrase& read = imported.Phrases()[i];
    const Phrase& source = original_.Phrases()[i];
    SCOPED_TRACE(source.name);
    EXPECT_EQ(read.name, source.name);
    EXPECT_TRUE(read.locked);
    EXPECT_FALSE(read.source.has_value());
    ASSERT_TRUE(read.encoding.has_value());
    ASSERT_TRUE(source.encoding.has_value());
    EXPECT_EQ(read.encoding->frames, lpc::EnsureStop(source.encoding->frames));
  }
  EXPECT_EQ(imported.Allocation().PhromsUsed(),
            original_.Allocation().PhromsUsed());

  const std::vector<std::vector<std::uint8_t>> again =
      Export(imported, PhromPreset(kLayoutConcatenated), "reexported");
  ASSERT_EQ(again.size(), 1U);
  EXPECT_EQ(again[0], exported[0]);
}

TEST_P(PhromImportRoundTripTest, SeparateImagesImportAsTheConcatenatedOneDoes) {
  const std::vector<std::vector<std::uint8_t>> separate =
      Export(original_, PhromPreset(kLayoutSeparate), "separate");
  ASSERT_FALSE(separate.empty());
  std::vector<std::uint8_t> joined;
  std::vector<std::string> names;
  for (const std::vector<std::uint8_t>& image : separate) {
    joined.insert(joined.end(), image.begin(), image.end());
    StatusOr<std::vector<Phrase>> phrases =
        ImportWords(image, {names.begin(), names.end()});
    ASSERT_TRUE(phrases.IsOk()) << phrases.GetStatus().ToString();
    for (const Phrase& phrase : *phrases) {
      names.push_back(phrase.name);
    }
  }
  const Project imported = ImportedProject(joined);
  ASSERT_EQ(imported.Phrases().size(), names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    EXPECT_EQ(imported.Phrases()[i].name, names[i]);
  }
}

TEST_P(PhromImportRoundTripTest, ImportedPhrasesAreNeverReEncoded) {
  const std::vector<std::vector<std::uint8_t>> exported =
      Export(original_, PhromPreset(kLayoutConcatenated), "exported");
  ASSERT_EQ(exported.size(), 1U);
  Project imported = ImportedProject(exported[0]);
  ASSERT_FALSE(imported.Phrases().empty());
  const Project before = imported;

  // Any call to the encoder fails the test.
  StrictMock<testing::MockEncoder> encoder;
  EncodingCache cache(encoder);
  for (const Phrase& phrase : imported.Phrases()) {
    const StatusOr<bool> current =
        EncodingCache::IsCurrent(phrase, imported.Settings().default_encoder);
    ASSERT_TRUE(current.IsOk());
    EXPECT_TRUE(*current);
  }
  const RefreshReport report = cache.Refresh(imported, /*force=*/true);
  EXPECT_EQ(report.encoded, 0);
  EXPECT_TRUE(report.failures.empty());
  const StatusOr<EncodingChange> changed = cache.ChangeSettings(
      imported, imported.Phrases()[0].id, {{"target_rms", "18"}});
  EXPECT_FALSE(changed.IsOk());
  EXPECT_EQ(imported, before);

  // Saved and loaded again, they are still locked and unchanged.
  const fs::path saved = fs::path(directory_.path().toStdString()) / "saved";
  DirectoryProjectStore store(saved);
  ASSERT_TRUE(serializer_.Save(imported, store).IsOk());
  const StatusOr<Project> reloaded = serializer_.Load(store);
  ASSERT_TRUE(reloaded.IsOk()) << reloaded.GetStatus().ToString();
  ASSERT_EQ(reloaded->Phrases().size(), imported.Phrases().size());
  for (std::size_t i = 0; i < imported.Phrases().size(); ++i) {
    EXPECT_TRUE(reloaded->Phrases()[i].locked);
    EXPECT_EQ(reloaded->Phrases()[i].encoding->frames,
              imported.Phrases()[i].encoding->frames);
  }
}

INSTANTIATE_TEST_SUITE_P(Fixtures, PhromImportRoundTripTest,
                         ::testing::Values("phrom/two-phroms",
                                           "phrom/one-phrom",
                                           "general/many-phrases"));

}  // namespace
}  // namespace speak5220::core::project
