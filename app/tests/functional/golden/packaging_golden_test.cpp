/*
 * File:        packaging_golden_test.cpp
 * Module:      packaging tests
 * Purpose:     Builds Acorn PHROM, TI indexed and Speak External images and
 *              compares them byte for byte with the reference images
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "core/packaging/acorn_phrom.h"
#include "core/packaging/phrom_image.h"
#include "core/packaging/speak_external_stream.h"
#include "core/packaging/ti_indexed.h"
#include "gtest/gtest.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

using core::packaging::AcornPhrom;
using core::packaging::AcornPhromOptions;
using core::packaging::PhromImage;
using core::packaging::SpeakExternalStream;
using core::packaging::TiIndexed;
using core::packaging::Word;
using core::packaging::WordTableEntry;

const std::filesystem::path kPackagingGolden =
    std::filesystem::path(SPEAK5220_GOLDEN_DIR) / "packaging";

class PackagingGoldenTest : public ::testing::Test {
 protected:
  void SetUp() override {
    const auto rows = ReadTable(kPackagingGolden / "words.tsv");
    ASSERT_TRUE(rows.IsOk()) << rows.GetStatus().ToString();
    for (const auto& row : *rows) {
      const auto frames = ReadFramesCsv(kPackagingGolden / row.at("frames"));
      ASSERT_TRUE(frames.IsOk()) << frames.GetStatus().ToString();
      words_.push_back({row.at("name"), *frames});
      stems_.push_back(std::filesystem::path(row.at("frames")).stem().string());
    }
    ASSERT_FALSE(words_.empty());
  }

  static std::vector<WordTableEntry> Table(const std::string& file) {
    std::vector<WordTableEntry> table;
    const auto rows = ReadTable(kPackagingGolden / file);
    EXPECT_TRUE(rows.IsOk()) << rows.GetStatus().ToString();
    if (rows.IsOk()) {
      for (const auto& row : *rows) {
        table.push_back({.number = std::stoi(row.at("number")),
                         .name = row.at("name"),
                         .offset = std::stoi(row.at("offset")),
                         .bytes = std::stoi(row.at("bytes")),
                         .frames = std::stoi(row.at("frames"))});
      }
    }
    return table;
  }

  static void ExpectImage(const core::StatusOr<PhromImage>& built,
                          const std::string& image, const std::string& table) {
    ASSERT_TRUE(built.IsOk()) << built.GetStatus().ToString();
    const auto expected = ReadBytes(kPackagingGolden / image);
    ASSERT_TRUE(expected.IsOk()) << expected.GetStatus().ToString();
    EXPECT_EQ(built->bytes, *expected) << image;
    EXPECT_EQ(built->table, Table(table)) << table;
  }

  std::vector<Word> words_;
  std::vector<std::string> stems_;
};

TEST_F(PackagingGoldenTest, AcornPhromMatchesReferenceImage) {
  AcornPhromOptions options;
  const auto rows = ReadTable(kPackagingGolden / "acorn_ascii.tsv");
  ASSERT_TRUE(rows.IsOk()) << rows.GetStatus().ToString();
  for (const auto& row : *rows) {
    options.ascii_map[std::stoi(row.at("code"))] = row.at("name");
  }
  ExpectImage(AcornPhrom::Build(words_, options), "acorn.bin",
              "acorn_table.tsv");
}

TEST_F(PackagingGoldenTest, TiIndexedMatchesReferenceImage) {
  ExpectImage(TiIndexed::Build(words_), "ti_indexed.bin",
              "ti_indexed_table.tsv");
}

TEST_F(PackagingGoldenTest, SpeakExternalStreamsMatchReference) {
  for (std::size_t i = 0; i < words_.size(); ++i) {
    SCOPED_TRACE(words_[i].name);
    const auto stream = SpeakExternalStream::Build(words_[i].frames);
    ASSERT_TRUE(stream.IsOk()) << stream.GetStatus().ToString();
    const auto expected =
        ReadBytes(kPackagingGolden / (stems_[i] + "_external.bin"));
    ASSERT_TRUE(expected.IsOk()) << expected.GetStatus().ToString();
    EXPECT_EQ(*stream, *expected);
  }
}

}  // namespace
}  // namespace speak5220::testing
