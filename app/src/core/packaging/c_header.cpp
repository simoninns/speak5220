/*
 * File:        c_header.cpp
 * Module:      packaging
 * Purpose:     Writes speech data as a C header: PHROM images with their
 *              word table, or one Speak External stream per phrase
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/packaging/c_header.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace speak5220::core::packaging {
namespace {

// Bytes per line of an array.
constexpr std::size_t kBytesPerLine = 12;

bool IsLetter(char c) {
  return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}
bool IsDigit(char c) { return c >= '0' && c <= '9'; }

std::string Upper(std::string_view text) {
  std::string out(text);
  std::ranges::transform(out, out.begin(), [](char c) {
    return (c >= 'a' && c <= 'z') ? static_cast<char>(c - 'a' + 'A') : c;
  });
  return out;
}

std::string Lower(std::string_view text) {
  std::string out(text);
  std::ranges::transform(out, out.begin(), [](char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  });
  return out;
}

Status CheckIdentifier(std::string_view identifier, std::string_view what) {
  const bool valid = !identifier.empty() && !IsDigit(identifier.front()) &&
                     std::ranges::all_of(identifier, [](char c) {
                       return IsLetter(c) || IsDigit(c) || c == '_';
                     });
  if (!valid) {
    return InvalidArgumentError(fmt::format(
        R"({} "{}" is not a C identifier (letters, digits and "_", not )"
        "starting with a digit)",
        what, identifier));
  }
  return OkStatus();
}

std::string_view Plural(std::size_t count) { return count == 1 ? "" : "s"; }

// Text that cannot end or break out of a /* */ comment.
std::string CommentText(std::string_view text) {
  std::string out;
  for (std::size_t i = 0; i < text.size(); ++i) {
    const char c = text[i];
    if (c == '\n' || c == '\r') {
      out += ' ';
    } else if (c == '*' && i + 1 < text.size() && text[i + 1] == '/') {
      out += "* ";
    } else {
      out += c;
    }
  }
  return out;
}

// The comment at the top: the SPDX line, what the file holds and the name
// mapping (plan §4).
void AppendComment(std::string& out, const CHeaderInfo& info,
                   std::string_view contents,
                   std::span<const CHeaderName> names,
                   std::string_view constant_prefix) {
  out += "/*\n";
  out += fmt::format(" * SPDX-License-Identifier: {}\n", info.licence);
  out += " *\n";
  out +=
      fmt::format(" * TMS5220 speech data from the Speak5220 project \"{}\":\n",
                  CommentText(info.project_name));
  out += fmt::format(" * {}.\n", contents);
  out += " * This file is generated: export it again rather than edit it.\n";
  if (!names.empty()) {
    std::size_t width = 0;
    for (const CHeaderName& name : names) {
      width = std::max(width, name.name.size());
    }
    out += " *\n";
    out += " * Phrase names and their C identifiers:\n";
    for (const CHeaderName& name : names) {
      out += fmt::format(" *   {:<{}}  {}{}\n", name.name, width,
                         constant_prefix, name.identifier);
    }
  }
  out += " */\n\n";
}

void AppendBytes(std::string& out, std::span<const std::uint8_t> bytes) {
  for (std::size_t i = 0; i < bytes.size(); ++i) {
    if (i % kBytesPerLine == 0) {
      out += "   ";
    }
    out += fmt::format(" 0x{:02X},", bytes[i]);
    if (i % kBytesPerLine == kBytesPerLine - 1 || i + 1 == bytes.size()) {
      out += '\n';
    }
  }
}

void AppendWordEnum(std::string& out, std::string_view lower,
                    std::string_view word_prefix,
                    std::span<const CHeaderName> names) {
  out += fmt::format("enum {}_word {{\n", lower);
  for (const CHeaderName& name : names) {
    out += fmt::format("  {}{},\n", word_prefix, name.identifier);
  }
  out += "};\n\n";
}

}  // namespace

Status CHeader::CheckInfo(const CHeaderInfo& info) {
  if (Status status = CheckIdentifier(info.prefix, "the C name prefix");
      !status.IsOk()) {
    return status;
  }
  const bool licence_valid =
      info.licence.find_first_not_of(' ') != std::string::npos &&
      std::ranges::all_of(info.licence, [](char c) {
        return IsLetter(c) || IsDigit(c) || c == ' ' || c == '.' || c == '+' ||
               c == '-' || c == '(' || c == ')';
      });
  if (!licence_valid) {
    return InvalidArgumentError(fmt::format(
        R"(the licence "{}" is not an SPDX identifier (letters, digits, )"
        "spaces and \". + - ( )\")",
        info.licence));
  }
  return OkStatus();
}

StatusOr<std::string> CHeader::ForPhroms(const CHeaderInfo& info,
                                         std::span<const PhromImage> images,
                                         std::span<const CHeaderName> names) {
  if (Status status = CheckInfo(info); !status.IsOk()) {
    return status;
  }
  std::size_t words = 0;
  for (const PhromImage& image : images) {
    for (const WordTableEntry& entry : image.table) {
      if (words >= names.size() || names[words].name != entry.name) {
        return InvalidArgumentError(
            fmt::format("no C identifier for the word {}", entry.name));
      }
      ++words;
    }
  }
  if (words != names.size()) {
    return InvalidArgumentError("more C identifiers than words");
  }

  const std::string upper = Upper(info.prefix);
  const std::string lower = Lower(info.prefix);
  const std::string word_prefix = upper + "_WORD_";
  std::string out;
  AppendComment(out, info,
                fmt::format("{} Acorn PHROM image{} of {} bytes", images.size(),
                            Plural(images.size()), kPhromSize),
                names, word_prefix);
  out += fmt::format("#ifndef {}_H_\n#define {}_H_\n\n", upper, upper);
  out += "#include <stdint.h>\n\n";
  out += fmt::format("#define {}_PHROMS {}\n", upper, images.size());
  out += fmt::format("#define {}_PHROM_SIZE {}\n", upper, kPhromSize);
  out += fmt::format("#define {}_WORDS {}\n\n", upper, words);

  for (std::size_t i = 0; i < images.size(); ++i) {
    out += fmt::format("static const uint8_t {}_phrom_{}[{}_PHROM_SIZE] = {{\n",
                       lower, i + 1, upper);
    AppendBytes(out, images[i].bytes);
    out += "};\n\n";
  }

  if (words > 0) {
    AppendWordEnum(out, lower, word_prefix, names);
    out += fmt::format(
        "/* A word's PHROM (from 1), its word number, and the offset and\n"
        "   length of its data in the PHROM. */\n"
        "struct {}_entry {{\n"
        "  uint8_t phrom;\n"
        "  uint16_t number;\n"
        "  uint16_t offset;\n"
        "  uint16_t length;\n"
        "  const char *name;\n"
        "}};\n\n",
        lower);
    out += fmt::format("static const struct {}_entry {}_words[{}_WORDS] = {{\n",
                       lower, lower, upper);
    std::size_t at = 0;
    for (std::size_t i = 0; i < images.size(); ++i) {
      for (const WordTableEntry& entry : images[i].table) {
        out += fmt::format("  {{{}, {}, 0x{:04X}, {}, \"{}\"}},  /* {}{} */\n",
                           i + 1, entry.number, entry.offset, entry.bytes,
                           entry.name, word_prefix, names[at].identifier);
        ++at;
      }
    }
    out += "};\n\n";
  }
  out += fmt::format("#endif  /* {}_H_ */\n", upper);
  return out;
}

StatusOr<std::string> CHeader::ForStreams(
    const CHeaderInfo& info, std::span<const CHeaderStream> streams) {
  if (Status status = CheckInfo(info); !status.IsOk()) {
    return status;
  }
  for (const CHeaderStream& stream : streams) {
    if (stream.bytes.size() > kMaxCHeaderStreamBytes) {
      return ResourceExhaustedError(fmt::format(
          "{} needs {} bytes, more than a C header's stream holds ({})",
          stream.name.name, stream.bytes.size(), kMaxCHeaderStreamBytes));
    }
  }
  std::vector<CHeaderName> names;
  names.reserve(streams.size());
  for (const CHeaderStream& stream : streams) {
    names.push_back(stream.name);
  }

  const std::string upper = Upper(info.prefix);
  const std::string lower = Lower(info.prefix);
  const std::string word_prefix = upper + "_WORD_";
  const std::string length_prefix = upper + "_LENGTH_";
  std::string out;
  AppendComment(out, info,
                fmt::format("{} Speak External stream{}, each written to "
                            "the chip's FIFO byte by byte as it is",
                            streams.size(), Plural(streams.size())),
                names, word_prefix);
  out += fmt::format("#ifndef {}_H_\n#define {}_H_\n\n", upper, upper);
  out += "#include <stdint.h>\n\n";
  out += fmt::format("#define {}_WORDS {}\n\n", upper, streams.size());

  for (const CHeaderStream& stream : streams) {
    const std::string& identifier = stream.name.identifier;
    out += fmt::format("#define {}{} {}\n", length_prefix, identifier,
                       stream.bytes.size());
    out += fmt::format("static const uint8_t {}_word_{}[{}{}] = {{\n", lower,
                       Lower(identifier), length_prefix, identifier);
    AppendBytes(out, stream.bytes);
    out += "};\n\n";
  }

  if (!streams.empty()) {
    AppendWordEnum(out, lower, word_prefix, names);
    out += fmt::format(
        "struct {}_entry {{\n"
        "  const char *name;\n"
        "  const uint8_t *data;\n"
        "  uint16_t length;\n"
        "}};\n\n",
        lower);
    out += fmt::format("static const struct {}_entry {}_words[{}_WORDS] = {{\n",
                       lower, lower, upper);
    for (const CHeaderStream& stream : streams) {
      const std::string& identifier = stream.name.identifier;
      out += fmt::format("  {{\"{}\", {}_word_{}, {}{}}},\n", stream.name.name,
                         lower, Lower(identifier), length_prefix, identifier);
    }
    out += "};\n\n";
  }
  out += fmt::format("#endif  /* {}_H_ */\n", upper);
  return out;
}

}  // namespace speak5220::core::packaging
