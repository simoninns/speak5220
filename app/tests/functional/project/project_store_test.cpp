/*
 * File:        project_store_test.cpp
 * Module:      project tests
 * Purpose:     Functional tests of the ZIP and directory project stores:
 *              round trips, backups, interrupted saves, unsafe archives and
 *              saving under a locale with a decimal comma
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_store.h"

#include <zip.h>

#include <QTemporaryDir>
#include <array>
#include <clocale>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <locale>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "core/audio/pcm24.h"
#include "core/audio/sndfile_wav_io.h"
#include "core/lpc/frame.h"
#include "core/project/directory_project_store.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_serializer.h"
#include "core/project/zip_project_store.h"
#include "core/quality/quality_assessor.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::core::project {
namespace {

namespace fs = std::filesystem;
using testing::EncodedPhrase;
using ::testing::HasSubstr;
using testing::LockedPhrase;
using testing::ProjectWith;

std::vector<std::uint8_t> Bytes(std::string_view text) {
  return {text.begin(), text.end()};
}

std::vector<std::uint8_t> ReadAll(const fs::path& path) {
  std::ifstream file(path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  return {text.begin(), text.end()};
}

std::set<std::string> Listing(const fs::path& directory) {
  std::set<std::string> names;
  for (const fs::directory_entry& entry : fs::directory_iterator(directory)) {
    names.insert(entry.path().filename().string());
  }
  return names;
}

const ProjectFiles& FirstVersion() {
  static const ProjectFiles kFiles = {
      {"project.yaml", Bytes("format: \"speak5220-project\"\nversion: 1\n")},
      {"phrases/x/source.wav", std::vector<std::uint8_t>(5000, 0x11)},
      {"notes/read me.txt", Bytes("first")},
  };
  return kFiles;
}

const ProjectFiles& SecondVersion() {
  static const ProjectFiles kFiles = {
      {"project.yaml",
       Bytes("format: \"speak5220-project\"\nversion: 1\n#2\n")},
      {"phrases/y/source.wav", std::vector<std::uint8_t>(9000, 0x22)},
  };
  return kFiles;
}

// A project with audio, frames of every kind, quoted names and options
// with decimal fractions.
Project SampleProject() {
  Phrase phrase = EncodedPhrase(1, "NO", 40);
  phrase.encoding->frames = {
      lpc::Frame::Voiced(9, 34, {12, 20, 8, 7, 9, 8, 7, 4, 3, 2}),
      lpc::Frame::Repeat(9, 35), lpc::Frame::Unvoiced(4, {20, 18, 9, 6}),
      lpc::Frame::Silence(), lpc::Frame::Stop()};
  phrase.encoding->quality =
      CachedQuality{.measures_version = quality::kMeasuresVersion,
                    .lsd_db = 3.25,
                    .lsd_over_2db = 81.5,
                    .lsd_over_4db = 27.75,
                    .gross_pitch_error = 5.5,
                    .f0_rmse_cents = 60.125,
                    .voicing_error = 9.875,
                    .estoi = 0.75,
                    .score = 0.6875};
  phrase.audio = {0.0F, 0.123456F, -0.5F, 0.999F};
  // The project stores audio on the 24-bit grid.
  audio::QuantiseToPcm24(phrase.audio);
  phrase.source->duration = 1.25;
  phrase.encoder = {{"voicing_threshold", "0.35"}, {"pre_emphasis", "0.9375"}};
  Project project = ProjectWith({phrase, LockedPhrase(2, "O'CLOCK", 30)});
  ProjectSettings settings = project.Settings();
  settings.name = "Sample, \xc3\xa9t\xc3\xa9";
  settings.default_encoder.lag_window_hz = 62.5;
  settings.ascii_map = {{' ', "NO"}};
  EXPECT_TRUE(project.SetSettings(settings).IsOk());
  return project;
}

class ProjectStoreTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(directory_.isValid()); }

  fs::path Path(const std::string& name) const {
    return fs::path(directory_.path().toStdString()) / name;
  }

  Status Save(const Project& project, IProjectStore& store) {
    return serializer_.Save(project, store);
  }

  StatusOr<Project> Load(IProjectStore& store) {
    return serializer_.Load(store);
  }

  QTemporaryDir directory_;
  audio::SndfileWavCodec codec_;
  ProjectSerializer serializer_{codec_};
};

TEST_F(ProjectStoreTest, ZipFileKeepsEveryFile) {
  ZipProjectStore store(Path("p.s5proj"));
  ASSERT_TRUE(store.Write(FirstVersion()).IsOk());
  const StatusOr<ProjectFiles> files = store.Read();
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  EXPECT_EQ(*files, FirstVersion());
}

TEST_F(ProjectStoreTest, DirectoryKeepsEveryFile) {
  DirectoryProjectStore store(Path("p"));
  ASSERT_TRUE(store.Write(FirstVersion()).IsOk());
  EXPECT_TRUE(fs::exists(Path("p") / "notes" / "read me.txt"));
  const StatusOr<ProjectFiles> files = store.Read();
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();
  EXPECT_EQ(*files, FirstVersion());
}

TEST_F(ProjectStoreTest, SavingTheSameProjectGivesTheSameArchive) {
  ZipProjectStore first(Path("a.s5proj"));
  ZipProjectStore second(Path("b.s5proj"));
  ASSERT_TRUE(Save(SampleProject(), first).IsOk());
  ASSERT_TRUE(Save(SampleProject(), second).IsOk());
  EXPECT_EQ(ReadAll(Path("a.s5proj")), ReadAll(Path("b.s5proj")));
}

TEST_F(ProjectStoreTest, ZipAndDirectoryFormsLoadIdentically) {
  const Project project = SampleProject();
  ZipProjectStore zip(Path("p.s5proj"));
  DirectoryProjectStore directory(Path("p"));
  ASSERT_TRUE(Save(project, zip).IsOk());
  ASSERT_TRUE(Save(project, directory).IsOk());
  const StatusOr<Project> from_zip = Load(zip);
  const StatusOr<Project> from_directory = Load(directory);
  ASSERT_TRUE(from_zip.IsOk()) << from_zip.GetStatus().ToString();
  ASSERT_TRUE(from_directory.IsOk()) << from_directory.GetStatus().ToString();
  EXPECT_EQ(*from_zip, *from_directory);
  EXPECT_EQ(*from_zip, project);
  EXPECT_EQ(*zip.Read(), *directory.Read());
}

TEST_F(ProjectStoreTest, MakeProjectStoreChoosesTheFormByExtension) {
  ASSERT_TRUE(MakeProjectStore(Path("p.s5proj"))->Write(FirstVersion()).IsOk());
  ASSERT_TRUE(MakeProjectStore(Path("p"))->Write(FirstVersion()).IsOk());
  EXPECT_TRUE(fs::is_regular_file(Path("p.s5proj")));
  EXPECT_TRUE(fs::is_directory(Path("p")));
}

TEST_F(ProjectStoreTest, SavingKeepsThePreviousVersion) {
  ZipProjectStore zip(Path("p.s5proj"));
  ASSERT_TRUE(zip.Write(FirstVersion()).IsOk());
  ASSERT_TRUE(zip.Write(SecondVersion()).IsOk());
  EXPECT_EQ(*zip.Read(), SecondVersion());
  EXPECT_EQ(*ZipProjectStore(Path("p.s5proj~")).Read(), FirstVersion());

  DirectoryProjectStore directory(Path("d"));
  ASSERT_TRUE(directory.Write(FirstVersion()).IsOk());
  ASSERT_TRUE(directory.Write(SecondVersion()).IsOk());
  EXPECT_EQ(*directory.Read(), SecondVersion());
  EXPECT_EQ(*DirectoryProjectStore(Path("d~")).Read(), FirstVersion());
  EXPECT_FALSE(fs::exists(Path("d.saving")));
}

TEST_F(ProjectStoreTest, InterruptedSaveLeavesThePreviousFileIntact) {
  ZipProjectStore store(Path("p.s5proj"));
  ASSERT_TRUE(Save(SampleProject(), store).IsOk());
  const std::vector<std::uint8_t> before = ReadAll(Path("p.s5proj"));

  // Stop part-way through writing the new archive, as a crash would.
  int calls = 0;
  store.SetCancelCheck([&calls] { return ++calls > 2; });
  Project changed = SampleProject();
  ASSERT_TRUE(changed.RenamePhrase(testing::TestId(1), "YES").IsOk());
  const Status status = Save(changed, store);
  EXPECT_FALSE(status.IsOk());
  EXPECT_GT(calls, 2);

  EXPECT_EQ(ReadAll(Path("p.s5proj")), before);
  const StatusOr<Project> loaded = Load(store);
  ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  EXPECT_EQ(*loaded, SampleProject());
  // No half-written file is left behind.
  EXPECT_EQ(Listing(Path("")),
            (std::set<std::string>{"p.s5proj", "p.s5proj~"}));
}

TEST_F(ProjectStoreTest, RefusesAnArchiveWithAnUnsafePath) {
  for (const char* name : {"../evil.txt", "/abs.txt", "a/../../evil.txt",
                           "C:/evil.txt", "a\\..\\evil.txt"}) {
    const fs::path path = Path("bad.s5proj");
    fs::remove(path);
    int error = 0;
    zip_t* archive = zip_open(path.string().c_str(), ZIP_CREATE, &error);
    ASSERT_NE(archive, nullptr);
    static constexpr std::array<char, 1> kData = {'x'};
    zip_source_t* source =
        zip_source_buffer(archive, kData.data(), kData.size(), 0);
    ASSERT_NE(source, nullptr);
    ASSERT_GE(zip_file_add(archive, name, source, ZIP_FL_ENC_RAW), 0) << name;
    ASSERT_EQ(zip_close(archive), 0);

    const StatusOr<ProjectFiles> files = ZipProjectStore(path).Read();
    ASSERT_FALSE(files.IsOk()) << name;
    EXPECT_THAT(files.GetStatus().Message(), HasSubstr("unsafe path")) << name;
  }
  EXPECT_FALSE(fs::exists(Path("evil.txt")));
}

TEST_F(ProjectStoreTest, RefusesToWriteAnUnsafePath) {
  ProjectFiles files = FirstVersion();
  files["../evil.txt"] = Bytes("x");
  EXPECT_FALSE(ZipProjectStore(Path("p.s5proj")).Write(files).IsOk());
  EXPECT_FALSE(DirectoryProjectStore(Path("p")).Write(files).IsOk());
  EXPECT_FALSE(fs::exists(Path("p.s5proj")));
  EXPECT_FALSE(fs::exists(Path("p")));
  EXPECT_FALSE(fs::exists(Path("evil.txt")));
}

TEST_F(ProjectStoreTest, RefusesAFileThatIsNotAnArchive) {
  std::ofstream(Path("junk.s5proj"), std::ios::binary) << "not a zip file";
  const StatusOr<ProjectFiles> files =
      ZipProjectStore(Path("junk.s5proj")).Read();
  EXPECT_EQ(files.GetStatus().Code(), StatusCode::kDataLoss);
  EXPECT_EQ(ZipProjectStore(Path("missing.s5proj")).Read().GetStatus().Code(),
            StatusCode::kNotFound);
  EXPECT_EQ(DirectoryProjectStore(Path("missing")).Read().GetStatus().Code(),
            StatusCode::kNotFound);
}

TEST_F(ProjectStoreTest, NeverReplacesADirectoryThatIsNotAProject) {
  fs::create_directories(Path("documents"));
  std::ofstream(Path("documents") / "letter.txt") << "keep me";
  const Status status =
      DirectoryProjectStore(Path("documents")).Write(FirstVersion());
  EXPECT_EQ(status.Code(), StatusCode::kFailedPrecondition);
  EXPECT_TRUE(fs::exists(Path("documents") / "letter.txt"));
  EXPECT_FALSE(ZipProjectStore(Path("documents")).Write(FirstVersion()).IsOk());
}

#ifndef _WIN32
TEST_F(ProjectStoreTest, RefusesASymbolicLinkInAProjectDirectory) {
  DirectoryProjectStore store(Path("p"));
  ASSERT_TRUE(store.Write(FirstVersion()).IsOk());
  std::ofstream(Path("secret.txt")) << "outside";
  fs::create_symlink(Path("secret.txt"), Path("p") / "link.txt");
  const StatusOr<ProjectFiles> files = store.Read();
  ASSERT_FALSE(files.IsOk());
  EXPECT_THAT(files.GetStatus().Message(), HasSubstr("symbolic link"));
}
#endif

// A numpunct with a decimal comma and "." grouping, as in de_DE.
class DecimalComma : public std::numpunct<char> {
 protected:
  char do_decimal_point() const override { return ','; }
  char do_thousands_sep() const override { return '.'; }
  std::string do_grouping() const override { return "\3"; }
};

// Switches the C++ global locale to a decimal comma, and the C locale to
// de_DE where the system has it, for the life of the object.
class DecimalCommaLocale {
 public:
  DecimalCommaLocale()
      : previous_(std::locale::global(
            std::locale(std::locale::classic(), new DecimalComma))),
        previous_c_(std::setlocale(LC_ALL, nullptr)) {
    std::setlocale(LC_NUMERIC, "de_DE.UTF-8");
  }
  ~DecimalCommaLocale() {
    std::locale::global(previous_);
    std::setlocale(LC_ALL, previous_c_.c_str());
  }
  DecimalCommaLocale(const DecimalCommaLocale&) = delete;
  DecimalCommaLocale& operator=(const DecimalCommaLocale&) = delete;

 private:
  std::locale previous_;
  std::string previous_c_;
};

TEST_F(ProjectStoreTest,
       ProjectSavedUnderADecimalCommaLocaleReloadsIdentically) {
  const Project project = SampleProject();
  ZipProjectStore store(Path("p.s5proj"));
  {
    const DecimalCommaLocale locale;
    std::ostringstream check;
    check << 1234.5;
    ASSERT_EQ(check.str(), "1.234,5") << "the locale did not take effect";
    ASSERT_TRUE(Save(project, store).IsOk());
    const StatusOr<Project> loaded = Load(store);
    ASSERT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
    EXPECT_EQ(*loaded, project);
  }
  const StatusOr<ProjectFiles> files = store.Read();
  ASSERT_TRUE(files.IsOk());
  const std::vector<std::uint8_t>& yaml_bytes = files->at("project.yaml");
  const std::string yaml(yaml_bytes.begin(), yaml_bytes.end());
  EXPECT_THAT(yaml, HasSubstr("lag_window_hz: 62.5\n"));
  EXPECT_THAT(yaml, HasSubstr("voicing_threshold: 0.35\n"));
  EXPECT_THAT(yaml, HasSubstr("duration: 1.25\n"));
  // And it loads the same in the C locale.
  const StatusOr<Project> loaded = Load(store);
  ASSERT_TRUE(loaded.IsOk());
  EXPECT_EQ(*loaded, project);
}

}  // namespace
}  // namespace speak5220::core::project
