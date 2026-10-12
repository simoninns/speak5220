/*
 * File:        dfs_disc.cpp
 * Module:      bbc
 * Purpose:     Writes Acorn DFS disc images: a catalogue and files per
 *              side, single-sided (.ssd) or double-sided (.dsd)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/bbc/dfs_disc.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace speak5220::core::bbc {
namespace {

// The catalogue's layout (Acorn DFS Manual): sector 0 holds the first 8
// characters of the title, then the files' names; sector 1 the last 4
// characters of the title, the cycle number, the number of entries × 8,
// the boot option and the side's sector count, then the files' addresses,
// lengths and start sectors. Entries are in descending order of start
// sector.
constexpr std::size_t kEntryBytes = 8;
constexpr std::size_t kTitleInSector0 = 8;
constexpr std::size_t kEntriesOffset = 8;
constexpr std::size_t kEntryCountOffset = 5;
constexpr std::size_t kOptionsOffset = 6;
constexpr std::size_t kSectorCountOffset = 7;
constexpr int kBootOptionShift = 4;
constexpr std::uint8_t kLockedBit = 0x80;
constexpr std::uint32_t kAddressMask = 0x3FFFF;

constexpr std::string_view kRefusedInNames = " #*.:\"";

bool Printable(char c) { return c > ' ' && c <= '~'; }

Status CheckName(const DfsFile& file) {
  const bool good =
      !file.name.empty() && file.name.size() <= DfsDisc::kMaxNameLength &&
      std::ranges::all_of(file.name,
                          [](char c) {
                            return Printable(c) && kRefusedInNames.find(c) ==
                                                       std::string_view::npos;
                          }) &&
      Printable(file.directory) &&
      kRefusedInNames.find(file.directory) == std::string_view::npos;
  if (!good) {
    return InvalidArgumentError(fmt::format(
        R"("{}.{}" is not a DFS file name: one to {} printable characters, )"
        R"(none of them space or # * . : ")",
        file.directory, file.name, DfsDisc::kMaxNameLength));
  }
  return OkStatus();
}

std::size_t SectorsFor(std::size_t bytes) {
  return (bytes + DfsDisc::kSectorBytes - 1) / DfsDisc::kSectorBytes;
}

void PutLittleEndian16(std::vector<std::uint8_t>& image, std::size_t at,
                       std::uint32_t value) {
  image[at] = static_cast<std::uint8_t>(value & 0xFF);
  image[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

// Bits 16 and 17 of a value, in the place the mixed byte keeps them.
std::uint8_t HighBits(std::uint32_t value, int shift) {
  return static_cast<std::uint8_t>(((value >> 16) & 0x03) << shift);
}

}  // namespace

StatusOr<std::vector<std::uint8_t>> DfsDisc::Side(const DfsSide& side) {
  if (side.title.size() > kMaxTitleLength ||
      !std::ranges::all_of(side.title,
                           [](char c) { return c >= ' ' && c <= '~'; })) {
    return InvalidArgumentError(fmt::format(
        R"("{}" is not a DFS disc title: at most {} printable characters)",
        side.title, kMaxTitleLength));
  }
  if (side.boot_option < 0 || side.boot_option > kMaxBootOption) {
    return InvalidArgumentError(
        fmt::format("the DFS boot option is 0 to {}, not {}", kMaxBootOption,
                    side.boot_option));
  }
  if (side.files.size() > static_cast<std::size_t>(kMaxFiles)) {
    return ResourceExhaustedError(
        fmt::format("a DFS disc side holds at most {} files, not {}", kMaxFiles,
                    side.files.size()));
  }
  std::set<std::pair<char, std::string>> names;
  std::size_t sectors = kCatalogueSectors;
  for (const DfsFile& file : side.files) {
    if (Status status = CheckName(file); !status.IsOk()) {
      return status;
    }
    if (!names.emplace(file.directory, file.name).second) {
      return InvalidArgumentError(
          fmt::format("two files on one DFS side are called {}.{}",
                      file.directory, file.name));
    }
    sectors += SectorsFor(file.bytes.size());
  }
  if (sectors > static_cast<std::size_t>(kSectorsPerSide)) {
    return ResourceExhaustedError(fmt::format(
        "the files need {} sectors, more than a DFS disc side holds ({})",
        sectors, kSectorsPerSide));
  }

  std::vector<std::uint8_t> image(kSideBytes, 0);
  std::string title = side.title;
  title.resize(kMaxTitleLength, ' ');
  std::copy_n(title.begin(), kTitleInSector0, image.begin());
  std::copy_n(title.begin() + kTitleInSector0,
              kMaxTitleLength - kTitleInSector0, image.begin() + kSectorBytes);
  image[kSectorBytes + kEntryCountOffset] =
      static_cast<std::uint8_t>(side.files.size() * kEntryBytes);
  image[kSectorBytes + kOptionsOffset] = static_cast<std::uint8_t>(
      (side.boot_option << kBootOptionShift) | (kSectorsPerSide >> 8));
  image[kSectorBytes + kSectorCountOffset] =
      static_cast<std::uint8_t>(kSectorsPerSide & 0xFF);

  std::size_t sector = kCatalogueSectors;
  for (std::size_t i = 0; i < side.files.size(); ++i) {
    const DfsFile& file = side.files[i];
    std::ranges::copy(file.bytes, image.begin() + static_cast<std::ptrdiff_t>(
                                                      sector * kSectorBytes));
    // The last file laid out is the first entry.
    const std::size_t entry =
        kEntriesOffset + (side.files.size() - 1 - i) * kEntryBytes;
    std::string name = file.name;
    name.resize(kMaxNameLength, ' ');
    std::ranges::copy(name, image.begin() + static_cast<std::ptrdiff_t>(entry));
    image[entry + kMaxNameLength] =
        static_cast<std::uint8_t>(static_cast<std::uint8_t>(file.directory) |
                                  (file.locked ? kLockedBit : 0));
    const std::size_t info = kSectorBytes + entry;
    const std::uint32_t load = file.load_address & kAddressMask;
    const std::uint32_t exec = file.exec_address & kAddressMask;
    const auto length = static_cast<std::uint32_t>(file.bytes.size());
    PutLittleEndian16(image, info, load);
    PutLittleEndian16(image, info + 2, exec);
    PutLittleEndian16(image, info + 4, length);
    image[info + 6] =
        static_cast<std::uint8_t>(HighBits(exec, 6) | HighBits(length, 4) |
                                  HighBits(load, 2) | ((sector >> 8) & 0x03));
    image[info + 7] = static_cast<std::uint8_t>(sector & 0xFF);
    sector += SectorsFor(file.bytes.size());
  }
  return image;
}

StatusOr<std::vector<std::uint8_t>> DfsDisc::SingleSided(const DfsSide& side) {
  return Side(side);
}

StatusOr<std::vector<std::uint8_t>> DfsDisc::DoubleSided(const DfsSide& side0,
                                                         const DfsSide& side1) {
  std::array<std::vector<std::uint8_t>, 2> sides;
  const std::array<const DfsSide*, 2> inputs = {&side0, &side1};
  for (std::size_t i = 0; i < sides.size(); ++i) {
    StatusOr<std::vector<std::uint8_t>> side = Side(*inputs[i]);
    if (!side.IsOk()) {
      return Status(side.GetStatus().Code(),
                    fmt::format("side {}: {}", i, side.GetStatus().Message()));
    }
    sides[i] = *std::move(side);
  }
  constexpr std::size_t kTrackBytes = kSectorsPerTrack * kSectorBytes;
  std::vector<std::uint8_t> image;
  image.reserve(2 * kSideBytes);
  for (int track = 0; track < kTracks; ++track) {
    for (const std::vector<std::uint8_t>& side : sides) {
      const auto from =
          side.begin() + static_cast<std::ptrdiff_t>(
                             static_cast<std::size_t>(track) * kTrackBytes);
      image.insert(image.end(), from,
                   from + static_cast<std::ptrdiff_t>(kTrackBytes));
    }
  }
  return image;
}

}  // namespace speak5220::core::bbc
