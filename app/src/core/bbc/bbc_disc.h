/*
 * File:        bbc_disc.h
 * Module:      bbc
 * Purpose:     Builds the BBC Micro disc: the player program, and each
 *              PHROM's phrase data and index on a double-sided DFS image
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_BBC_BBC_DISC_H_
#define SPEAK5220_CORE_BBC_BBC_DISC_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::bbc {

// A phrase on the disc: its name and its Speak External stream (speak5220
// datasheet §12.9).
struct BbcDiscPhrase {
  std::string name = {};
  std::vector<std::uint8_t> stream = {};
};

// One PHROM's phrases, in its order, and its title.
struct BbcDiscPhrom {
  std::string title = {};
  std::vector<BbcDiscPhrase> phrases = {};
};

// The BBC Micro disc of plan §6: a double-sided 40-track Acorn DFS image
// whose fixed player program (SPEAK, from speak.bas) finds the PHROMs by
// their index files and speaks their phrases with Speak External.
//
// Drive 0 (side 0) holds !BOOT, SPEAK, PHROM1-4 and INDEX1-4, drive 2
// (side 1) PHROM5-8 and INDEX5-8; only the PHROMs given are written.
// PHROMn is PHROM n's streams, one after another in its order. INDEXn is
// a 32-byte header, PHROM n's title in bytes 0-29 (padded with spaces)
// and its phrase count in bytes 30-31, then one 20-byte record per
// phrase: the name in bytes 0-15 (padded with spaces), and the stream's
// offset in PHROMn and its length in bytes 16-17 and 18-19. Numbers are
// little-endian. Stateless; thread-safe.
class BbcDisc {
 public:
  // Plan §6: the disc holds at most 8 PHROMs, 4 on each side.
  static constexpr int kMaxPhroms = 8;
  static constexpr int kPhromsPerSide = 4;
  // A PHROM's data file fits the player's 16 KB buffer.
  static constexpr std::size_t kMaxDataBytes = 16384;
  static constexpr std::size_t kIndexHeaderBytes = 32;
  static constexpr std::size_t kIndexTitleLength = 30;
  static constexpr std::size_t kRecordBytes = 20;
  static constexpr std::size_t kRecordNameLength = 16;

  // PHROMn: the phrases' streams. Fails if a phrase has no stream, or the
  // streams exceed kMaxDataBytes.
  static StatusOr<std::vector<std::uint8_t>> PhromData(
      const BbcDiscPhrom& phrom);

  // INDEXn: the header and a record per phrase. The title is cut to
  // kIndexTitleLength, with characters that are not printable ASCII made
  // "?". Fails for a name longer than kRecordNameLength or not printable
  // ASCII, and as PhromData().
  static StatusOr<std::vector<std::uint8_t>> Index(const BbcDiscPhrom& phrom);

  // The player's BASIC source.
  static std::string_view PlayerSource();

  // The disc title for a project's name: printable ASCII, other
  // characters made "?", cut to 12 characters.
  static std::string DiscTitle(std::string_view project_name);

  // The .dsd image: `title` (DiscTitle()) on both sides, side 0 with boot
  // option 3 (*EXEC !BOOT, which chains SPEAK on Shift+Break), and the
  // files above for `phroms`, PHROM 1 first. Fails for no PHROMs or more
  // than kMaxPhroms, and as Index() and PhromData(), naming the PHROM.
  static StatusOr<std::vector<std::uint8_t>> Build(
      std::string_view title, std::span<const BbcDiscPhrom> phroms);
};

}  // namespace speak5220::core::bbc

#endif  // SPEAK5220_CORE_BBC_BBC_DISC_H_
