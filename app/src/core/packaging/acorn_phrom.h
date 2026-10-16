/*
 * File:        acorn_phrom.h
 * Module:      packaging
 * Purpose:     Builds and reads 16 KB Acorn PHROM images: header, pointer
 *              table, word names and word data
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PACKAGING_ACORN_PHROM_H_
#define SPEAK5220_CORE_PACKAGING_ACORN_PHROM_H_

#include <cstdint>
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

// The bytes a PHROM header holds for its strings, with a null after each
// (bytes 2-55; speak5220 datasheet §12.8).
inline constexpr int kHeaderStringsCapacity = 54;

// Whether `strings` can be a PHROM header's: each non-empty printable
// ASCII (a host reads the strings up to the first empty one), and together,
// with a null after each, at most kHeaderStringsCapacity bytes. Pure;
// O(total length).
Status ValidateHeaderStrings(std::span<const std::string> strings);

// The Acorn PHROM layout of speak5220 datasheet §12.8: a bit-reversed
// 64-byte header, word number N's 14-bit pointer at byte 2N (numbers 32 to
// 126 are the ASCII entries, words proper from 127), 0xFF after the last
// pointer, then each word's name (backwards, each character shifted left
// and bit-reversed) followed by its packed data. Stateless; thread-safe.
class AcornPhrom {
 public:
  // The first word number after the 95 ASCII entries.
  static constexpr int kFirstWordNumber = 127;

  // Builds the image. Fails if a name is not ASCII, the strings are not
  // valid (ValidateHeaderStrings()), an ASCII entry is out of range or names no
  // word, or the words do not fit. Re-implements tms5220enc.py acornPhrom().
  // O(total bytes).
  static StatusOr<PhromImage> Build(std::span<const Word> words,
                                    const AcornPhromOptions& options);

  // Reads the words proper of one bank (`image` must be kPhromSize bytes),
  // in word-number order: the numbers after the ASCII entries (from
  // kFirstWordNumber when, as in every image Build() makes and in the
  // Acorn PHROM A, there are 95) to the end of the pointer table. A word's
  // name is read from the bytes between the end of the word before it in
  // the bank (or the pointer table's end marker) and its data, nearest the
  // data first, for as long as they decode to printable ASCII; so a name
  // is empty when the image holds none. Fails, as untrusted input, for a
  // wrong size, a header that is not an Acorn PHROM's, counts that do not
  // fit the bank, a pointer into the header or the pointer table, or a
  // word with no stop code within the bank. O(bytes + words log words).
  static StatusOr<std::vector<ReadWord>> Read(
      std::span<const std::uint8_t> image);

  // Read(), with the header's strings (the null-terminated printable ASCII
  // strings of bytes 2-55, up to the first empty one) and the ASCII
  // entries that point at a word. Entries that point elsewhere, such as
  // the shared stop code Build() writes, are left out. Fails as Read().
  static StatusOr<ReadPhrom> ReadImage(std::span<const std::uint8_t> image);
};

}  // namespace speak5220::core::packaging

#endif  // SPEAK5220_CORE_PACKAGING_ACORN_PHROM_H_
