/*
 * File:        dfs_disc.h
 * Module:      bbc
 * Purpose:     Writes Acorn DFS disc images: a catalogue and files per
 *              side, single-sided (.ssd) or double-sided (.dsd)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_BBC_DFS_DISC_H_
#define SPEAK5220_CORE_BBC_DFS_DISC_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::bbc {

// A file on a DFS disc.
struct DfsFile {
  // One to seven printable characters, none of them space or # * . : "
  std::string name = {};
  char directory = '$';
  // 18-bit addresses: bits 16 and 17 set mean the I/O processor, as in
  // 0xFF1900 for a BASIC program saved with PAGE at &1900.
  std::uint32_t load_address = 0;
  std::uint32_t exec_address = 0;
  std::vector<std::uint8_t> bytes = {};
  bool locked = false;
};

// One side of a disc: its own drive, with its own catalogue.
struct DfsSide {
  // At most 12 printable characters.
  std::string title = {};
  // What Shift+Break does: 0 nothing, 1 *LOAD !BOOT, 2 *RUN !BOOT,
  // 3 *EXEC !BOOT.
  int boot_option = 0;
  // In the order they are laid out on the side, from sector 2.
  std::vector<DfsFile> files = {};
};

// Acorn DFS disc images of 40 tracks of ten 256-byte sectors per side
// (Acorn DFS Manual): the catalogue in sectors 0 and 1, the files after
// it, each in whole consecutive sectors. Stateless; thread-safe.
class DfsDisc {
 public:
  static constexpr int kTracks = 40;
  static constexpr int kSectorsPerTrack = 10;
  static constexpr std::size_t kSectorBytes = 256;
  static constexpr int kSectorsPerSide = kTracks * kSectorsPerTrack;
  static constexpr std::size_t kSideBytes = kSectorsPerSide * kSectorBytes;
  // The catalogue's two sectors hold 31 entries of 8 bytes each.
  static constexpr int kMaxFiles = 31;
  static constexpr int kCatalogueSectors = 2;
  static constexpr std::size_t kMaxTitleLength = 12;
  static constexpr std::size_t kMaxNameLength = 7;
  static constexpr int kMaxBootOption = 3;

  // One side's 100 KB: the catalogue, then each file, unused sectors 0.
  // Fails for a bad title, name or boot option, two files of the same name,
  // more than kMaxFiles files, or files that do not fit. O(bytes).
  static StatusOr<std::vector<std::uint8_t>> Side(const DfsSide& side);

  // A single-sided image (.ssd): Side().
  static StatusOr<std::vector<std::uint8_t>> SingleSided(const DfsSide& side);

  // A double-sided image (.dsd), 200 KB: the sides' tracks interleaved,
  // track 0 of side 0, track 0 of side 1, track 1 of side 0, and so on, as
  // BBC emulators and floppy emulators read it. Side 0 is drive 0, side 1
  // drive 2. Fails as Side(), naming the side.
  static StatusOr<std::vector<std::uint8_t>> DoubleSided(const DfsSide& side0,
                                                         const DfsSide& side1);
};

}  // namespace speak5220::core::bbc

#endif  // SPEAK5220_CORE_BBC_DFS_DISC_H_
