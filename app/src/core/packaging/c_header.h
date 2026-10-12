/*
 * File:        c_header.h
 * Module:      packaging
 * Purpose:     Writes speech data as a C header: PHROM images with their
 *              word table, or one Speak External stream per phrase
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PACKAGING_C_HEADER_H_
#define SPEAK5220_CORE_PACKAGING_C_HEADER_H_

#include <cstdint>
#include <span>
#include <string>
#include <vector>

#include "core/packaging/phrom_image.h"
#include "core/util/status.h"

namespace speak5220::core::packaging {

// What names a header and what its comment says.
struct CHeaderInfo {
  // The C identifier every name in the header starts with, in any case:
  // "speech" gives speech_phrom_1, SPEECH_WORD_HELLO and the include guard
  // SPEECH_H_. Letters, digits and "_", not starting with a digit.
  std::string prefix;
  // The project's name, for the comment. "*/" and line breaks are written
  // so that they cannot end the comment.
  std::string project_name;
  // The SPDX licence identifier (or expression) of the generated file:
  // letters, digits, spaces and ". + - ( )".
  std::string licence;
};

// A phrase's name and its C identifier (project::CIdentifiersForNames()).
struct CHeaderName {
  std::string name;
  std::string identifier;
};

// A phrase as a Speak External stream (SpeakExternalStream::Build()).
struct CHeaderStream {
  CHeaderName name;
  std::vector<std::uint8_t> bytes;
};

// The longest stream a Speak External header holds: its length is a
// uint16_t.
inline constexpr std::size_t kMaxCHeaderStreamBytes = 65535;

// Writes and checks the C headers of plan §6. Every header has an SPDX
// line, a comment naming the project and listing each phrase's name with
// its identifier, an include guard, and <stdint.h>. Arrays are
// `static const`, so the header can be included by more than one file.
// The names a header defines cannot clash: phrases' constants all start
// <PREFIX>_WORD_ and their lengths <PREFIX>_LENGTH_, and nothing else does.
// The same input always gives the same text: there is no date. Stateless;
// thread-safe.
class CHeader {
 public:
  // Checks the prefix and the licence.
  static Status CheckInfo(const CHeaderInfo& info);

  // One `<prefix>_phrom_N[16384]` array per image, an enum of the words
  // (<PREFIX>_WORD_<identifier>) and a table giving each word's PHROM
  // (from 1), word number, the offset and length of its data in the
  // PHROM, and its name. `names` holds one entry per word of the images'
  // tables, in order. Fails for a bad prefix or licence, or a name that
  // does not match its table entry. O(total bytes).
  static StatusOr<std::string> ForPhroms(const CHeaderInfo& info,
                                         std::span<const PhromImage> images,
                                         std::span<const CHeaderName> names);

  // One `<prefix>_word_<identifier>` array per stream, with its length
  // (<PREFIX>_LENGTH_<identifier>), an enum of the words and a table of
  // each word's name, data and length. Fails for a bad prefix or licence,
  // or a stream longer than kMaxCHeaderStreamBytes. O(total bytes).
  static StatusOr<std::string> ForStreams(
      const CHeaderInfo& info, std::span<const CHeaderStream> streams);
};

}  // namespace speak5220::core::packaging

#endif  // SPEAK5220_CORE_PACKAGING_C_HEADER_H_
