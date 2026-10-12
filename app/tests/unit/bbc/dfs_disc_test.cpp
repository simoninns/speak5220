/*
 * File:        dfs_disc_test.cpp
 * Module:      bbc tests
 * Purpose:     Unit tests of the DFS disc image writer: the catalogue's
 *              fields, where files and sides lie, and its checks
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/bbc/dfs_disc.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::bbc {
namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kSector = DfsDisc::kSectorBytes;
constexpr std::size_t kTrack = DfsDisc::kSectorsPerTrack * kSector;

DfsFile File(std::string name, std::size_t bytes, std::uint8_t fill) {
  return {.name = std::move(name),
          .directory = '$',
          .load_address = 0,
          .exec_address = 0,
          .bytes = Bytes(bytes, fill),
          .locked = false};
}

// Two files: BOOT (14 bytes) in sector 2, then DATA (300 bytes) in sectors
// 3 and 4.
DfsSide TwoFiles() {
  DfsSide side{.title = "HELLO WORLD!", .boot_option = 3, .files = {}};
  side.files.push_back(File("!BOOT", 14, 0x11));
  DfsFile data = File("DATA", 300, 0x22);
  data.directory = 'W';
  data.load_address = 0xFF1900;
  data.exec_address = 0xFF8023;
  data.locked = true;
  side.files.push_back(data);
  return side;
}

std::string Text(const Bytes& bytes, std::size_t at, std::size_t length) {
  return {bytes.begin() + static_cast<std::ptrdiff_t>(at),
          bytes.begin() + static_cast<std::ptrdiff_t>(at + length)};
}

TEST(DfsDiscTest, CatalogueHoldsTheTitleBootOptionAndSize) {
  const Bytes side = DfsDisc::Side(TwoFiles()).Value();
  ASSERT_EQ(side.size(), DfsDisc::kSideBytes);
  EXPECT_EQ(Text(side, 0, 8), "HELLO WO");
  EXPECT_EQ(Text(side, kSector, 4), "RLD!");
  EXPECT_EQ(side[kSector + 5], 2 * 8);  // two entries
  // Boot option 3 in bits 4-5; 400 sectors (0x190) in bits 0-1 and byte 7.
  EXPECT_EQ(side[kSector + 6], 0x31);
  EXPECT_EQ(side[kSector + 7], 0x90);
}

TEST(DfsDiscTest, EntriesAreInDescendingOrderOfStartSector) {
  const Bytes side = DfsDisc::Side(TwoFiles()).Value();
  // DATA, laid out last, is the first entry; locked, in directory W.
  EXPECT_EQ(Text(side, 8, 7), "DATA   ");
  EXPECT_EQ(side[15], 'W' | 0x80);
  EXPECT_EQ(Text(side, 16, 7), "!BOOT  ");
  EXPECT_EQ(side[23], '$');
  const std::size_t data = kSector + 8;
  EXPECT_EQ(side[data + 0], 0x00);  // load 0xFF1900
  EXPECT_EQ(side[data + 1], 0x19);
  EXPECT_EQ(side[data + 2], 0x23);  // exec 0xFF8023
  EXPECT_EQ(side[data + 3], 0x80);
  EXPECT_EQ(side[data + 4], 0x2C);  // length 300
  EXPECT_EQ(side[data + 5], 0x01);
  // Exec bits 16-17 in 6-7, length's in 4-5, load's in 2-3, start's in 0-1.
  EXPECT_EQ(side[data + 6], 0xCC);
  EXPECT_EQ(side[data + 7], 3);
  const std::size_t boot = kSector + 16;
  EXPECT_EQ(side[boot + 4], 14);
  EXPECT_EQ(side[boot + 6], 0x00);
  EXPECT_EQ(side[boot + 7], 2);
}

TEST(DfsDiscTest, FilesLieInTheirSectors) {
  const Bytes side = DfsDisc::Side(TwoFiles()).Value();
  EXPECT_EQ(side[2 * kSector], 0x11);
  EXPECT_EQ(side[2 * kSector + 13], 0x11);
  EXPECT_EQ(side[2 * kSector + 14], 0x00);
  EXPECT_EQ(side[3 * kSector], 0x22);
  EXPECT_EQ(side[3 * kSector + 299], 0x22);
  EXPECT_EQ(side[3 * kSector + 300], 0x00);
}

TEST(DfsDiscTest, ShortTitlesArePaddedWithSpaces) {
  DfsSide side = TwoFiles();
  side.title = "ABC";
  const Bytes image = DfsDisc::Side(side).Value();
  EXPECT_EQ(Text(image, 0, 8), "ABC     ");
  EXPECT_EQ(Text(image, kSector, 4), "    ");
}

TEST(DfsDiscTest, DoubleSidedInterleavesTracks) {
  DfsSide other{.title = "SIDE1", .boot_option = 0, .files = {}};
  // 12 sectors: track 0's sectors 2-9, then track 1's sectors 0-3.
  other.files.push_back(File("BIG", 12 * kSector, 0x33));
  const Bytes side0 = DfsDisc::Side(TwoFiles()).Value();
  const Bytes side1 = DfsDisc::Side(other).Value();
  const Bytes disc = DfsDisc::DoubleSided(TwoFiles(), other).Value();
  ASSERT_EQ(disc.size(), 2 * DfsDisc::kSideBytes);
  for (int track = 0; track < DfsDisc::kTracks; ++track) {
    const auto t = static_cast<std::size_t>(track);
    EXPECT_EQ(
        Bytes(disc.begin() + static_cast<std::ptrdiff_t>(2 * t * kTrack),
              disc.begin() + static_cast<std::ptrdiff_t>((2 * t + 1) * kTrack)),
        Bytes(side0.begin() + static_cast<std::ptrdiff_t>(t * kTrack),
              side0.begin() + static_cast<std::ptrdiff_t>((t + 1) * kTrack)))
        << "side 0, track " << track;
    EXPECT_EQ(
        Bytes(disc.begin() + static_cast<std::ptrdiff_t>((2 * t + 1) * kTrack),
              disc.begin() + static_cast<std::ptrdiff_t>((2 * t + 2) * kTrack)),
        Bytes(side1.begin() + static_cast<std::ptrdiff_t>(t * kTrack),
              side1.begin() + static_cast<std::ptrdiff_t>((t + 1) * kTrack)))
        << "side 1, track " << track;
  }
  // Side 1's catalogue follows side 0's first track.
  EXPECT_EQ(Text(disc, kTrack, 5), "SIDE1");
  // Its file runs on into its second track, after side 0's.
  EXPECT_EQ(disc[3 * kTrack + 3 * kSector], 0x33);
  EXPECT_EQ(disc[3 * kTrack + 4 * kSector], 0x00);
}

TEST(DfsDiscTest, AFullSideFits) {
  DfsSide side{.title = "", .boot_option = 0, .files = {}};
  side.files.push_back(File("ALL", 398 * kSector, 0x44));
  EXPECT_TRUE(DfsDisc::Side(side).IsOk());
  side.files.push_back(File("ONE", 1, 0x44));
  EXPECT_EQ(DfsDisc::Side(side).GetStatus().Code(),
            StatusCode::kResourceExhausted);
}

TEST(DfsDiscTest, HoldsAtMost31Files) {
  DfsSide side{.title = "", .boot_option = 0, .files = {}};
  for (int i = 0; i < DfsDisc::kMaxFiles; ++i) {
    side.files.push_back(File("F" + std::to_string(i), 10, 0));
  }
  EXPECT_TRUE(DfsDisc::Side(side).IsOk());
  side.files.push_back(File("MORE", 10, 0));
  EXPECT_EQ(DfsDisc::Side(side).GetStatus().Code(),
            StatusCode::kResourceExhausted);
}

TEST(DfsDiscTest, RefusesBadNamesTitlesAndBootOptions) {
  for (const char* name :
       {"", "EIGHTCH8", "A B", "A.B", "A#", "A*", "A:", "A\"", "\x7F"}) {
    DfsSide side{.title = "", .boot_option = 0, .files = {}};
    side.files.push_back(File(name, 1, 0));
    EXPECT_EQ(DfsDisc::Side(side).GetStatus().Code(),
              StatusCode::kInvalidArgument)
        << name;
  }
  DfsSide same{.title = "", .boot_option = 0, .files = {}};
  same.files.push_back(File("SAME", 1, 0));
  same.files.push_back(File("SAME", 1, 0));
  EXPECT_FALSE(DfsDisc::Side(same).IsOk());

  DfsSide title = TwoFiles();
  title.title = "THIRTEEN CHRS";
  EXPECT_FALSE(DfsDisc::Side(title).IsOk());
  title.title = "TAB\t";
  EXPECT_FALSE(DfsDisc::Side(title).IsOk());

  DfsSide boot = TwoFiles();
  boot.boot_option = 4;
  EXPECT_FALSE(DfsDisc::Side(boot).IsOk());
  const StatusOr<Bytes> disc = DfsDisc::DoubleSided(TwoFiles(), boot);
  ASSERT_FALSE(disc.IsOk());
  EXPECT_NE(disc.GetStatus().Message().find("side 1"), std::string::npos);
}

}  // namespace
}  // namespace speak5220::core::bbc
