/*
 * File:        bbc_disc_test.cpp
 * Module:      bbc tests
 * Purpose:     Unit tests of the BBC Micro disc: the PHROM data and index
 *              layouts, where each file goes, and the PHROM limit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/bbc/bbc_disc.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/bbc/basic_tokeniser.h"
#include "core/bbc/dfs_disc.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::bbc {
namespace {

using Bytes = std::vector<std::uint8_t>;

constexpr std::size_t kSector = DfsDisc::kSectorBytes;
constexpr std::size_t kTrack = DfsDisc::kSectorsPerTrack * kSector;

BbcDiscPhrom Phrom(const std::string& title, int phrases) {
  BbcDiscPhrom phrom{.title = title, .phrases = {}};
  for (int i = 0; i < phrases; ++i) {
    phrom.phrases.push_back(
        {.name = "W" + std::to_string(i + 1),
         .stream = Bytes(static_cast<std::size_t>(9 + i),
                         static_cast<std::uint8_t>(0xA0 + i))});
  }
  return phrom;
}

int Little16(const Bytes& bytes, std::size_t at) {
  return bytes[at] | (bytes[at + 1] << 8);
}

std::string Text(const Bytes& bytes, std::size_t at, std::size_t length) {
  return {bytes.begin() + static_cast<std::ptrdiff_t>(at),
          bytes.begin() + static_cast<std::ptrdiff_t>(at + length)};
}

// One side of a .dsd image.
Bytes SideOf(const Bytes& disc, std::size_t side) {
  Bytes bytes;
  for (std::size_t track = 0; track < DfsDisc::kTracks; ++track) {
    const auto from =
        disc.begin() + static_cast<std::ptrdiff_t>((2 * track + side) * kTrack);
    bytes.insert(bytes.end(), from, from + static_cast<std::ptrdiff_t>(kTrack));
  }
  return bytes;
}

// The side's file names, in the order they are laid out.
std::vector<std::string> Names(const Bytes& side) {
  std::vector<std::string> names;
  const std::size_t entries = side[kSector + 5] / 8;
  for (std::size_t i = entries; i-- > 0;) {
    std::string name = Text(side, 8 + 8 * i, 7);
    name.erase(name.find_last_not_of(' ') + 1);
    names.push_back(name);
  }
  return names;
}

// The bytes of the file called `name` on the side.
Bytes FileOf(const Bytes& side, const std::string& name) {
  const std::size_t entries = side[kSector + 5] / 8;
  for (std::size_t i = 0; i < entries; ++i) {
    std::string entry = Text(side, 8 + 8 * i, 7);
    entry.erase(entry.find_last_not_of(' ') + 1);
    if (entry == name) {
      const std::size_t info = kSector + 8 + 8 * i;
      const auto length = static_cast<std::size_t>(Little16(side, info + 4));
      const std::size_t start = side[info + 7] | ((side[info + 6] & 3) << 8);
      const auto from =
          side.begin() + static_cast<std::ptrdiff_t>(start * kSector);
      return {from, from + static_cast<std::ptrdiff_t>(length)};
    }
  }
  return {};
}

TEST(BbcDiscTest, PhromDataIsTheStreamsInOrder) {
  const Bytes data = BbcDisc::PhromData(Phrom("P", 2)).Value();
  // Filled in place: GCC 15 at -O3 wrongly reports insert() growing a
  // 9-byte vector as writing past it (-Warray-bounds).
  Bytes expected(9 + 10, 0xA1);
  std::fill_n(expected.begin(), 9, 0xA0);
  EXPECT_EQ(data, expected);
}

TEST(BbcDiscTest, IndexHasTheTitleCountAndARecordPerPhrase) {
  const Bytes index = BbcDisc::Index(Phrom("Speak5220 PHROM 1", 3)).Value();
  ASSERT_EQ(index.size(),
            BbcDisc::kIndexHeaderBytes + 3 * BbcDisc::kRecordBytes);
  EXPECT_EQ(Text(index, 0, 30), "Speak5220 PHROM 1             ");
  EXPECT_EQ(Little16(index, 30), 3);
  // Records of 20 bytes: the name padded to 16, the offset, the length.
  EXPECT_EQ(Text(index, 32, 16), "W1              ");
  EXPECT_EQ(Text(index, 72, 16), "W3              ");
  EXPECT_EQ(Little16(index, 48), 0);
  EXPECT_EQ(Little16(index, 50), 9);
  EXPECT_EQ(Little16(index, 52 + 16), 9);
  EXPECT_EQ(Little16(index, 52 + 18), 10);
  EXPECT_EQ(Little16(index, 72 + 16), 19);
  EXPECT_EQ(Little16(index, 72 + 18), 11);
}

TEST(BbcDiscTest, IndexTitleIsCutAndMadePrintable) {
  BbcDiscPhrom phrom = Phrom("A", 1);
  phrom.title = "Caf\xC3\xA9 " + std::string(40, 'x');
  const Bytes index = BbcDisc::Index(phrom).Value();
  EXPECT_EQ(Text(index, 0, 30), "Caf?? " + std::string(24, 'x'));
}

TEST(BbcDiscTest, RefusesNamesTheIndexCannotHold) {
  BbcDiscPhrom phrom = Phrom("A", 1);
  phrom.phrases[0].name = "SEVENTEEN LETTERS";
  EXPECT_EQ(BbcDisc::Index(phrom).GetStatus().Code(),
            StatusCode::kInvalidArgument);
  phrom.phrases[0].name = "";
  EXPECT_FALSE(BbcDisc::Index(phrom).IsOk());
}

TEST(BbcDiscTest, RefusesDataLargerThanThePlayersBuffer) {
  BbcDiscPhrom phrom{.title = "BIG", .phrases = {}};
  phrom.phrases.push_back({.name = "A", .stream = Bytes(16000, 1)});
  phrom.phrases.push_back({.name = "B", .stream = Bytes(384, 1)});
  EXPECT_TRUE(BbcDisc::PhromData(phrom).IsOk());
  phrom.phrases[1].stream.push_back(1);
  EXPECT_EQ(BbcDisc::PhromData(phrom).GetStatus().Code(),
            StatusCode::kResourceExhausted);
  phrom.phrases[1].stream.clear();
  EXPECT_FALSE(BbcDisc::PhromData(phrom).IsOk());
}

TEST(BbcDiscTest, OnePhromIsAllOnSideZero) {
  const std::vector<BbcDiscPhrom> phroms = {Phrom("P1", 2)};
  const Bytes disc = BbcDisc::Build("My project", phroms).Value();
  ASSERT_EQ(disc.size(), 2 * DfsDisc::kSideBytes);
  const Bytes side0 = SideOf(disc, 0);
  const Bytes side1 = SideOf(disc, 1);
  EXPECT_EQ(Names(side0),
            (std::vector<std::string>{"!BOOT", "SPEAK", "PHROM1", "INDEX1"}));
  EXPECT_TRUE(Names(side1).empty());
  // The title on both sides; Shift+Break runs !BOOT, which chains SPEAK.
  EXPECT_EQ(Text(side0, 0, 8) + Text(side0, kSector, 4), "My project  ");
  EXPECT_EQ(Text(side1, 0, 8) + Text(side1, kSector, 4), "My project  ");
  EXPECT_EQ(side0[kSector + 6] >> 4, 3);
  EXPECT_EQ(side1[kSector + 6] >> 4, 0);
  const Bytes boot = FileOf(side0, "!BOOT");
  EXPECT_EQ(std::string(boot.begin(), boot.end()), "CHAIN \"SPEAK\"\r");
  EXPECT_EQ(FileOf(side0, "SPEAK"),
            BasicTokeniser::Tokenise(BbcDisc::PlayerSource()).Value());
  EXPECT_EQ(FileOf(side0, "PHROM1"), BbcDisc::PhromData(phroms[0]).Value());
  EXPECT_EQ(FileOf(side0, "INDEX1"), BbcDisc::Index(phroms[0]).Value());
}

TEST(BbcDiscTest, PhromsFiveToEightAreOnSideOne) {
  std::vector<BbcDiscPhrom> phroms;
  for (int i = 1; i <= BbcDisc::kMaxPhroms; ++i) {
    phroms.push_back(Phrom("P" + std::to_string(i), i));
  }
  const Bytes disc = BbcDisc::Build("Eight", phroms).Value();
  const Bytes side0 = SideOf(disc, 0);
  const Bytes side1 = SideOf(disc, 1);
  EXPECT_EQ(Names(side0),
            (std::vector<std::string>{"!BOOT", "SPEAK", "PHROM1", "INDEX1",
                                      "PHROM2", "INDEX2", "PHROM3", "INDEX3",
                                      "PHROM4", "INDEX4"}));
  EXPECT_EQ(Names(side1),
            (std::vector<std::string>{"PHROM5", "INDEX5", "PHROM6", "INDEX6",
                                      "PHROM7", "INDEX7", "PHROM8", "INDEX8"}));
  EXPECT_EQ(FileOf(side1, "INDEX8"), BbcDisc::Index(phroms[7]).Value());
  EXPECT_EQ(FileOf(side1, "PHROM5"), BbcDisc::PhromData(phroms[4]).Value());
}

TEST(BbcDiscTest, HoldsOneToEightPhroms) {
  std::vector<BbcDiscPhrom> phroms(BbcDisc::kMaxPhroms + 1, Phrom("P", 1));
  EXPECT_EQ(BbcDisc::Build("Nine", phroms).GetStatus().Code(),
            StatusCode::kResourceExhausted);
  EXPECT_FALSE(BbcDisc::Build("None", {}).IsOk());
}

TEST(BbcDiscTest, NamesTheFailingPhrom) {
  std::vector<BbcDiscPhrom> phroms = {Phrom("P1", 1), Phrom("P2", 1)};
  phroms[1].phrases[0].name = "MUCH TOO LONG A NAME";
  const StatusOr<Bytes> disc = BbcDisc::Build("Bad", phroms);
  ASSERT_FALSE(disc.IsOk());
  EXPECT_NE(disc.GetStatus().Message().find("PHROM 2"), std::string::npos);
}

TEST(BbcDiscTest, DiscTitleIsPrintableAndCut) {
  EXPECT_EQ(BbcDisc::DiscTitle("Numbers"), "Numbers");
  EXPECT_EQ(BbcDisc::DiscTitle("A very long project name"), "A very long ");
  EXPECT_EQ(BbcDisc::DiscTitle("Tab\there"), "Tab?here");
}

TEST(BbcDiscTest, PlayerTokenisesAndFitsBesideTheBuffer) {
  const StatusOr<Bytes> player =
      BasicTokeniser::Tokenise(BbcDisc::PlayerSource());
  ASSERT_TRUE(player.IsOk()) << player.GetStatus().ToString();
  // Plan §6: with PAGE at &1900 and the 16 KB buffer below the MODE 7
  // screen at &7C00, the program, its variables and the routine share
  // about 8.75 KB; the player refuses to start with less than &900 bytes
  // above the program.
  constexpr std::size_t kBesideTheBuffer = 0x7C00 - 0x4000 - 0x1900;
  EXPECT_LT(player->size() + 0x900, kBesideTheBuffer);
}

}  // namespace
}  // namespace speak5220::core::bbc
