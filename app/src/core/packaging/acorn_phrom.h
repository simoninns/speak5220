/*
 * File:        acorn_phrom.h
 * Module:      packaging
 * Purpose:     Builds a 16 KB Acorn PHROM image: header, pointer table,
 *              word names and word data
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PACKAGING_ACORN_PHROM_H_
#define SPEAK5220_CORE_PACKAGING_ACORN_PHROM_H_

#include <map>
#include <span>
#include <string>
#include <vector>

#include "core/packaging/phrom_image.h"
#include "core/util/status.h"

namespace speak5220::core::packaging {

// The header strings and the "ASCII associated" entries of an image.
struct AcornPhromOptions {
  // Null-terminated in the header's bytes 2-55 (54 bytes with their
  // nulls). The BBC Micro checks that bytes 2-4 are "(C)" (Acorn Speech
  // System User Guide, Section 7).
  std::vector<std::string> strings = {"(C)2026 speak5220", "Speak5220 PHROM",
                                      "1.00"};
  // Maps a printable ASCII code (32-126) to a word's name. Entries mapping
  // to nothing point to a shared one-byte stop code.
  std::map<int, std::string> ascii_map;
};

// The Acorn PHROM layout of speak5220 datasheet §12.8: a bit-reversed
// 64-byte header, word number N's 14-bit pointer at byte 2N (numbers 32 to
// 126 are the ASCII entries, words proper from 127), 0xFF after the last
// pointer, then each word's name (backwards, each character shifted left
// and bit-reversed) followed by its packed data. Stateless; thread-safe.
class AcornPhrom {
 public:
  // The first word number after the 95 ASCII entries.
  static constexpr int kFirstWordNumber = 127;

  // Builds the image. Fails if a name is not ASCII, the strings are too
  // long, an ASCII entry is out of range or names no word, or the words do
  // not fit. Re-implements tms5220enc.py acornPhrom(). O(total bytes).
  static StatusOr<PhromImage> Build(std::span<const Word> words,
                                    const AcornPhromOptions& options);
};

}  // namespace speak5220::core::packaging

#endif  // SPEAK5220_CORE_PACKAGING_ACORN_PHROM_H_
