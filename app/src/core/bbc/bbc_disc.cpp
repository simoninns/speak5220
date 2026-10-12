/*
 * File:        bbc_disc.cpp
 * Module:      bbc
 * Purpose:     Builds the BBC Micro disc: the player program, and each
 *              PHROM's phrase data and index on a double-sided DFS image
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/bbc/bbc_disc.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/bbc/basic_tokeniser.h"
#include "core/bbc/dfs_disc.h"
#include "core/bbc/player_source.h"

namespace speak5220::core::bbc {
namespace {

// A BASIC program saved with PAGE at &1900 (Acorn DFS), in the I/O
// processor: what SAVE gives it.
constexpr std::uint32_t kProgramLoadAddress = 0xFF1900;
constexpr std::uint32_t kProgramExecAddress = 0xFF8023;
// Acorn DFS Manual: boot option 3 runs *EXEC !BOOT on Shift+Break.
constexpr int kExecBootOption = 3;
constexpr std::string_view kBootText = "CHAIN \"SPEAK\"\r";
constexpr std::size_t kIndexCountOffset = 30;

bool IsPrintable(char c) { return c >= ' ' && c <= '~'; }

std::string PrintableAscii(std::string_view text, std::size_t length) {
  std::string result(text.substr(0, length));
  std::ranges::replace_if(result, [](char c) { return !IsPrintable(c); }, '?');
  return result;
}

void PutLittleEndian16(std::vector<std::uint8_t>& bytes, std::size_t at,
                       std::size_t value) {
  bytes[at] = static_cast<std::uint8_t>(value & 0xFF);
  bytes[at + 1] = static_cast<std::uint8_t>((value >> 8) & 0xFF);
}

DfsFile DataFile(std::string name, std::vector<std::uint8_t> bytes) {
  return {.name = std::move(name),
          .directory = '$',
          .load_address = 0,
          .exec_address = 0,
          .bytes = std::move(bytes),
          .locked = false};
}

}  // namespace

StatusOr<std::vector<std::uint8_t>> BbcDisc::PhromData(
    const BbcDiscPhrom& phrom) {
  std::vector<std::uint8_t> data;
  for (const BbcDiscPhrase& phrase : phrom.phrases) {
    if (phrase.stream.empty()) {
      return InvalidArgumentError(
          fmt::format("{} has no Speak External stream", phrase.name));
    }
    data.insert(data.end(), phrase.stream.begin(), phrase.stream.end());
  }
  if (data.size() > kMaxDataBytes) {
    return ResourceExhaustedError(fmt::format(
        "the phrases' streams take {} bytes, more than the player's buffer "
        "holds ({})",
        data.size(), kMaxDataBytes));
  }
  return data;
}

StatusOr<std::vector<std::uint8_t>> BbcDisc::Index(const BbcDiscPhrom& phrom) {
  StatusOr<std::vector<std::uint8_t>> data = PhromData(phrom);
  if (!data.IsOk()) {
    return data.GetStatus();
  }
  std::vector<std::uint8_t> index(
      kIndexHeaderBytes + phrom.phrases.size() * kRecordBytes, ' ');
  const std::string title = PrintableAscii(phrom.title, kIndexTitleLength);
  std::ranges::copy(title, index.begin());
  PutLittleEndian16(index, kIndexCountOffset, phrom.phrases.size());
  std::size_t offset = 0;
  for (std::size_t i = 0; i < phrom.phrases.size(); ++i) {
    const BbcDiscPhrase& phrase = phrom.phrases[i];
    if (phrase.name.empty() || phrase.name.size() > kRecordNameLength ||
        !std::ranges::all_of(phrase.name, IsPrintable)) {
      return InvalidArgumentError(fmt::format(
          R"("{}" cannot name a phrase on the disc: 1 to {} printable )"
          "ASCII characters",
          phrase.name, kRecordNameLength));
    }
    const std::size_t record = kIndexHeaderBytes + i * kRecordBytes;
    std::ranges::copy(phrase.name,
                      index.begin() + static_cast<std::ptrdiff_t>(record));
    PutLittleEndian16(index, record + kRecordNameLength, offset);
    PutLittleEndian16(index, record + kRecordNameLength + 2,
                      phrase.stream.size());
    offset += phrase.stream.size();
  }
  return index;
}

std::string_view BbcDisc::PlayerSource() { return kPlayerSource; }

std::string BbcDisc::DiscTitle(std::string_view project_name) {
  return PrintableAscii(project_name, DfsDisc::kMaxTitleLength);
}

StatusOr<std::vector<std::uint8_t>> BbcDisc::Build(
    std::string_view title, std::span<const BbcDiscPhrom> phroms) {
  if (phroms.empty() || phroms.size() > static_cast<std::size_t>(kMaxPhroms)) {
    return ResourceExhaustedError(
        fmt::format("a BBC Micro disc holds 1 to {} PHROMs, not {}", kMaxPhroms,
                    phroms.size()));
  }
  StatusOr<std::vector<std::uint8_t>> player =
      BasicTokeniser::Tokenise(PlayerSource());
  if (!player.IsOk()) {
    return Status(player.GetStatus().Code(),
                  "the player program: " + player.GetStatus().Message());
  }
  const std::string disc_title = DiscTitle(title);
  std::array<DfsSide, 2> sides = {
      DfsSide{.title = disc_title, .boot_option = kExecBootOption, .files = {}},
      DfsSide{.title = disc_title, .boot_option = 0, .files = {}}};
  sides[0].files.push_back(DataFile(
      "!BOOT", std::vector<std::uint8_t>(kBootText.begin(), kBootText.end())));
  sides[0].files.push_back({.name = "SPEAK",
                            .directory = '$',
                            .load_address = kProgramLoadAddress,
                            .exec_address = kProgramExecAddress,
                            .bytes = *std::move(player),
                            .locked = false});
  for (std::size_t i = 0; i < phroms.size(); ++i) {
    const int number = static_cast<int>(i) + 1;
    StatusOr<std::vector<std::uint8_t>> data = PhromData(phroms[i]);
    StatusOr<std::vector<std::uint8_t>> index = Index(phroms[i]);
    for (const Status& status : {data.GetStatus(), index.GetStatus()}) {
      if (!status.IsOk()) {
        return Status(status.Code(),
                      fmt::format("PHROM {}: {}", number, status.Message()));
      }
    }
    DfsSide& side = sides[i / kPhromsPerSide];
    side.files.push_back(
        DataFile(fmt::format("PHROM{}", number), *std::move(data)));
    side.files.push_back(
        DataFile(fmt::format("INDEX{}", number), *std::move(index)));
  }
  return DfsDisc::DoubleSided(sides[0], sides[1]);
}

}  // namespace speak5220::core::bbc
