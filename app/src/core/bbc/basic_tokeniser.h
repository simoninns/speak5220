/*
 * File:        basic_tokeniser.h
 * Module:      bbc
 * Purpose:     Turns BBC BASIC II program text into the tokenised form
 *              BASIC saves
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_BBC_BASIC_TOKENISER_H_
#define SPEAK5220_CORE_BBC_BASIC_TOKENISER_H_

#include <array>
#include <cstdint>
#include <string_view>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::bbc {

// Tokenises a BBC BASIC II program as BASIC II does when the program is
// typed in, giving the bytes SAVE writes (PAGE to TOP). Each line is
// 0x0D, its number (high byte first), its length (these four bytes
// included), then its text, keywords replaced by their tokens; the program
// ends with 0x0D 0xFF. The rules follow BASIC II's own, and the tests check
// them against programs BASIC II tokenised. Stateless; thread-safe.
class BasicTokeniser {
 public:
  // The highest line number BASIC II accepts.
  static constexpr int kMaxLineNumber = 32767;
  // A line's length byte counts its four leading bytes.
  static constexpr int kMaxLineBytes = 255;

  // Tokenises `source`: one program line per text line ("\n", "\r\n" or
  // "\r"), each starting with its line number, in ascending order. Blank
  // lines are skipped. Keywords are recognised in full or abbreviated with
  // "." as BASIC II recognises them. Fails, naming the line, for a line with
  // no number or no text, a number out of order or above kMaxLineNumber, a
  // character that is not printable ASCII, or a line too long once
  // tokenised. O(length × keywords).
  static StatusOr<std::vector<std::uint8_t>> Tokenise(std::string_view source);

  // A line number written after GOTO, GOSUB and the like: 0x8D, then three
  // bytes holding the number's bits (BBC BASIC II).
  static std::array<std::uint8_t, 4> EncodeLineNumber(int number);
};

}  // namespace speak5220::core::bbc

#endif  // SPEAK5220_CORE_BBC_BASIC_TOKENISER_H_
