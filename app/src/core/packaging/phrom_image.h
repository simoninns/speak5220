/*
 * File:        phrom_image.h
 * Module:      packaging
 * Purpose:     The words placed in a 16 KB PHROM image, and the image with
 *              its word table
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PACKAGING_PHROM_IMAGE_H_
#define SPEAK5220_CORE_PACKAGING_PHROM_IMAGE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "core/lpc/frame.h"

namespace speak5220::core::packaging {

// One 16 KB bank (speak5220 datasheet §12.8).
inline constexpr std::size_t kPhromSize = 16384;

// A word to place in an image: its name and its frames. A stop frame is
// added if the frames have none.
struct Word {
  std::string name;
  std::vector<lpc::Frame> frames;
};

// Where a word was placed.
struct WordTableEntry {
  int number = 0;  // its word number in the image
  std::string name;
  int offset = 0;  // the byte offset of its data in the bank
  int bytes = 0;   // the length of its packed data
  int frames = 0;  // its frames, the stop included

  friend bool operator==(const WordTableEntry& a,
                         const WordTableEntry& b) = default;
};

// A finished image and its word table, one entry per word in order.
struct PhromImage {
  std::vector<std::uint8_t> bytes;
  std::vector<WordTableEntry> table;
};

}  // namespace speak5220::core::packaging

#endif  // SPEAK5220_CORE_PACKAGING_PHROM_IMAGE_H_
