/*
 * File:        bbc_disc_reader_test.cpp
 * Module:      bbc tests
 * Purpose:     Exports the BBC Micro disc of each bbc fixture through the
 *              CLI and reads it back with an independent DFS reader
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QByteArray>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QProcess>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/bbc/basic_tokeniser.h"
#include "core/bbc/bbc_disc.h"
#include "core/project/directory_project_store.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/project/project_serializer.h"
#include "core/util/sha256.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::bbc {
namespace {

namespace fs = std::filesystem;

constexpr int kProcessTimeoutMs = 120000;

// What the reader says a file on a side is.
struct ReadBack {
  int load = 0;
  int exec = 0;
  int length = 0;
  std::string sha256;
};

class BbcDiscReaderTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(directory_.isValid()); }

  // Runs `program` with `arguments`; the test fails, with its output, if it
  // does not succeed.
  static QByteArray Run(const QString& program, const QStringList& arguments) {
    QProcess process;
    process.start(program, arguments);
    EXPECT_TRUE(process.waitForFinished(kProcessTimeoutMs)) << "timed out";
    EXPECT_EQ(process.exitCode(), 0)
        << QString::fromUtf8(process.readAllStandardError()).toStdString();
    return process.readAllStandardOutput();
  }

  // The fixture's "disc" preset, exported by the CLI into the temporary
  // directory.
  fs::path ExportDisc(const std::string& fixture) {
    const fs::path project = fs::path(SPEAK5220_PROJECTS_DIR) / "bbc" / fixture;
    Run(QStringLiteral(SPEAK5220_CLI_PATH),
        {QStringLiteral("export"), QString::fromStdString(project.string()),
         QStringLiteral("--output"),
         directory_.filePath(QString::fromStdString(fixture)),
         QStringLiteral("--preset"), QStringLiteral("disc")});
    return fs::path(directory_.path().toStdString()) / fixture / "disc.dsd";
  }

  // Each side's catalogue, as dfs_catalogue.py reads it.
  static QJsonArray Sides(const fs::path& disc) {
    const QByteArray json =
        Run(QStringLiteral(SPEAK5220_PYTHON),
            {QStringLiteral(SPEAK5220_TEST_SUPPORT_DIR "/dfs_catalogue.py"),
             QString::fromStdString(disc.string())});
    return QJsonDocument::fromJson(json)
        .object()[QStringLiteral("sides")]
        .toArray();
  }

  // The files of a side, by name, in the order they are laid out.
  static std::vector<std::pair<std::string, ReadBack>> Files(
      const QJsonObject& side) {
    std::map<int, std::pair<std::string, ReadBack>> by_sector;
    for (const auto& value : side[QStringLiteral("files")].toArray()) {
      const QJsonObject file = value.toObject();
      by_sector[file[QStringLiteral("start_sector")].toInt()] = {
          file[QStringLiteral("name")].toString().toStdString(),
          {.load = file[QStringLiteral("load")].toInt(),
           .exec = file[QStringLiteral("exec")].toInt(),
           .length = file[QStringLiteral("length")].toInt(),
           .sha256 = file[QStringLiteral("sha256")].toString().toStdString()}};
    }
    std::vector<std::pair<std::string, ReadBack>> files;
    files.reserve(by_sector.size());
    for (auto& [sector, file] : by_sector) {
      files.push_back(std::move(file));
    }
    return files;
  }

  // The disc's PHROMs, as the export builds them from the fixture.
  std::vector<BbcDiscPhrom> Phroms(const std::string& fixture) {
    project::DirectoryProjectStore store(fs::path(SPEAK5220_PROJECTS_DIR) /
                                         "bbc" / fixture);
    StatusOr<project::Project> loaded = serializer_.Load(store);
    EXPECT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
    project_ = *std::move(loaded);
    std::vector<const project::Phrase*> phrases;
    for (const project::Phrase& phrase : project_.Phrases()) {
      phrases.push_back(&phrase);
    }
    return project::BuildBbcDiscPhroms(project_.Settings(), phrases).Value();
  }

  static std::string Hash(const StatusOr<std::vector<std::uint8_t>>& bytes) {
    return Sha256Hex(bytes.Value());
  }

  QTemporaryDir directory_;
  audio::SndfileWavCodec codec_;
  project::ProjectSerializer serializer_{codec_};
  project::Project project_;
};

TEST_F(BbcDiscReaderTest, OnePhromIsAllOnDriveZero) {
  if (std::string(SPEAK5220_PYTHON).empty()) {
    GTEST_SKIP() << "no Python 3 interpreter to run the reader";
  }
  const QJsonArray sides = Sides(ExportDisc("one-phrom"));
  ASSERT_EQ(sides.size(), 2);
  const QJsonObject side0 = sides[0].toObject();
  EXPECT_EQ(side0[QStringLiteral("title")].toString(),
            QStringLiteral("One PHROM"));
  EXPECT_EQ(side0[QStringLiteral("boot_option")].toInt(), 3);
  EXPECT_EQ(side0[QStringLiteral("sectors")].toInt(), 400);
  EXPECT_TRUE(sides[1].toObject()[QStringLiteral("files")].toArray().isEmpty());

  const std::vector<BbcDiscPhrom> phroms = Phroms("one-phrom");
  const auto files = Files(side0);
  ASSERT_EQ(files.size(), 4U);
  EXPECT_EQ(files[0].first, "!BOOT");
  EXPECT_EQ(files[0].second.length, 14);
  EXPECT_EQ(files[1].first, "SPEAK");
  // A BASIC program at PAGE &1900, in the I/O processor.
  EXPECT_EQ(files[1].second.load, 0x31900);
  EXPECT_EQ(files[1].second.exec, 0x38023);
  EXPECT_EQ(files[1].second.sha256,
            Hash(BasicTokeniser::Tokenise(BbcDisc::PlayerSource())));
  EXPECT_EQ(files[2].first, "PHROM1");
  EXPECT_EQ(files[2].second.sha256, Hash(BbcDisc::PhromData(phroms[0])));
  EXPECT_EQ(files[3].first, "INDEX1");
  EXPECT_EQ(files[3].second.sha256, Hash(BbcDisc::Index(phroms[0])));
}

TEST_F(BbcDiscReaderTest, PhromsFiveOnAreOnDriveTwo) {
  if (std::string(SPEAK5220_PYTHON).empty()) {
    GTEST_SKIP() << "no Python 3 interpreter to run the reader";
  }
  const QJsonArray sides = Sides(ExportDisc("both-sides"));
  ASSERT_EQ(sides.size(), 2);
  const std::vector<BbcDiscPhrom> phroms = Phroms("both-sides");
  ASSERT_EQ(phroms.size(), 5U);
  const auto side0 = Files(sides[0].toObject());
  const auto side1 = Files(sides[1].toObject());
  std::vector<std::string> names0;
  names0.reserve(side0.size());
  for (const auto& [name, file] : side0) {
    names0.push_back(name);
  }
  EXPECT_EQ(names0, (std::vector<std::string>{
                        "!BOOT", "SPEAK", "PHROM1", "INDEX1", "PHROM2",
                        "INDEX2", "PHROM3", "INDEX3", "PHROM4", "INDEX4"}));
  for (std::size_t i = 0; i < 4; ++i) {
    const std::size_t at = 2 + 2 * i;
    EXPECT_EQ(side0[at].second.sha256, Hash(BbcDisc::PhromData(phroms[i])));
    EXPECT_EQ(side0[at + 1].second.sha256, Hash(BbcDisc::Index(phroms[i])));
  }
  ASSERT_EQ(side1.size(), 2U);
  EXPECT_EQ(side1[0].first, "PHROM5");
  EXPECT_EQ(side1[0].second.sha256, Hash(BbcDisc::PhromData(phroms[4])));
  EXPECT_EQ(side1[1].first, "INDEX5");
  EXPECT_EQ(side1[1].second.sha256, Hash(BbcDisc::Index(phroms[4])));
  // Side 1 is drive 2: same title, no boot option.
  EXPECT_EQ(sides[1].toObject()[QStringLiteral("title")].toString(),
            QStringLiteral("Both sides"));
  EXPECT_EQ(sides[1].toObject()[QStringLiteral("boot_option")].toInt(), 0);
  // PHROM 1 holds the largest phrase a PHROM can.
  EXPECT_GT(side0[2].second.length, 16000);
}

}  // namespace
}  // namespace speak5220::core::bbc
