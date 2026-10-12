/*
 * File:        basic_tokeniser.cpp
 * Module:      bbc
 * Purpose:     Turns BBC BASIC II program text into the tokenised form
 *              BASIC saves
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/bbc/basic_tokeniser.h"

#include <spdlog/fmt/fmt.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>
#include <utility>
#include <vector>

namespace speak5220::core::bbc {
namespace {

// What a keyword does to the text after it (BBC BASIC II's keyword flags).
// Not tokenised when followed by a letter, digit or "_" (END in ENDX).
constexpr std::uint8_t kConditional = 0x01;
// Leaves the start of a statement (pseudo-variables after it are read).
constexpr std::uint8_t kMiddle = 0x02;
// Starts a statement again (after THEN, ELSE, LET, ERROR).
constexpr std::uint8_t kStart = 0x04;
// The name after FN or PROC is copied as it is.
constexpr std::uint8_t kFnProc = 0x08;
// Numbers after it are line numbers (GOTO, GOSUB, THEN, ELSE, ...).
constexpr std::uint8_t kLineNumber = 0x10;
// The rest of the line is copied as it is (REM, DATA).
constexpr std::uint8_t kRestOfLine = 0x20;
// At the start of a statement the token is 0x40 higher (TIME=0).
constexpr std::uint8_t kPseudoVariable = 0x40;

constexpr std::uint8_t kPseudoVariableStatement = 0x40;
constexpr std::uint8_t kLineNumberToken = 0x8D;
constexpr std::uint8_t kCarriageReturn = 0x0D;
constexpr std::uint8_t kEndOfProgram = 0xFF;
// 0x0D, the line number's two bytes and the length byte.
constexpr int kLineOverhead = 4;

struct KeywordEntry {
  std::string_view text;
  std::uint8_t token;
  std::uint8_t flags;
};

// BBC BASIC II's keyword table, in its order: the first keyword that
// matches wins, so ENDPROC comes before END. The statement forms of the
// pseudo-variables (token + 0x40) are not matched; kPseudoVariable gives
// them.
constexpr std::array<KeywordEntry, 121> kKeywords = {{
    {.text = "AND", .token = 0x80, .flags = 0},
    {.text = "ABS", .token = 0x94, .flags = 0},
    {.text = "ACS", .token = 0x95, .flags = 0},
    {.text = "ADVAL", .token = 0x96, .flags = 0},
    {.text = "ASC", .token = 0x97, .flags = 0},
    {.text = "ASN", .token = 0x98, .flags = 0},
    {.text = "ATN", .token = 0x99, .flags = 0},
    {.text = "AUTO", .token = 0xC6, .flags = kLineNumber},
    {.text = "BGET", .token = 0x9A, .flags = kConditional},
    {.text = "BPUT", .token = 0xD5, .flags = kConditional | kMiddle},
    {.text = "COLOUR", .token = 0xFB, .flags = kMiddle},
    {.text = "CALL", .token = 0xD6, .flags = kMiddle},
    {.text = "CHAIN", .token = 0xD7, .flags = kMiddle},
    {.text = "CHR$", .token = 0xBD, .flags = 0},
    {.text = "CLEAR", .token = 0xD8, .flags = kConditional},
    {.text = "CLOSE", .token = 0xD9, .flags = kConditional | kMiddle},
    {.text = "CLG", .token = 0xDA, .flags = kConditional},
    {.text = "CLS", .token = 0xDB, .flags = kConditional},
    {.text = "COS", .token = 0x9B, .flags = 0},
    {.text = "COUNT", .token = 0x9C, .flags = kConditional},
    {.text = "DATA", .token = 0xDC, .flags = kRestOfLine},
    {.text = "DEG", .token = 0x9D, .flags = 0},
    {.text = "DEF", .token = 0xDD, .flags = 0},
    {.text = "DELETE", .token = 0xC7, .flags = kLineNumber},
    {.text = "DIV", .token = 0x81, .flags = 0},
    {.text = "DIM", .token = 0xDE, .flags = kMiddle},
    {.text = "DRAW", .token = 0xDF, .flags = kMiddle},
    {.text = "ENDPROC", .token = 0xE1, .flags = kConditional},
    {.text = "END", .token = 0xE0, .flags = kConditional},
    {.text = "ENVELOPE", .token = 0xE2, .flags = kMiddle},
    {.text = "ELSE", .token = 0x8B, .flags = kStart | kLineNumber},
    {.text = "EVAL", .token = 0xA0, .flags = 0},
    {.text = "ERL", .token = 0x9E, .flags = kConditional},
    {.text = "ERROR", .token = 0x85, .flags = kStart},
    {.text = "EOF", .token = 0xC5, .flags = kConditional},
    {.text = "EOR", .token = 0x82, .flags = 0},
    {.text = "ERR", .token = 0x9F, .flags = kConditional},
    {.text = "EXP", .token = 0xA1, .flags = 0},
    {.text = "EXT", .token = 0xA2, .flags = kConditional},
    {.text = "FOR", .token = 0xE3, .flags = kMiddle},
    {.text = "FALSE", .token = 0xA3, .flags = kConditional},
    {.text = "FN", .token = 0xA4, .flags = kFnProc},
    {.text = "GOTO", .token = 0xE5, .flags = kMiddle | kLineNumber},
    {.text = "GET$", .token = 0xBE, .flags = 0},
    {.text = "GET", .token = 0xA5, .flags = 0},
    {.text = "GOSUB", .token = 0xE4, .flags = kMiddle | kLineNumber},
    {.text = "GCOL", .token = 0xE6, .flags = kMiddle},
    {.text = "HIMEM",
     .token = 0x93,
     .flags = kConditional | kMiddle | kPseudoVariable},
    {.text = "INPUT", .token = 0xE8, .flags = kMiddle},
    {.text = "IF", .token = 0xE7, .flags = kMiddle},
    {.text = "INKEY$", .token = 0xBF, .flags = 0},
    {.text = "INKEY", .token = 0xA6, .flags = 0},
    {.text = "INT", .token = 0xA8, .flags = 0},
    {.text = "INSTR(", .token = 0xA7, .flags = 0},
    {.text = "LIST", .token = 0xC9, .flags = kLineNumber},
    {.text = "LINE", .token = 0x86, .flags = 0},
    {.text = "LOAD", .token = 0xC8, .flags = kMiddle},
    {.text = "LOMEM",
     .token = 0x92,
     .flags = kConditional | kMiddle | kPseudoVariable},
    {.text = "LOCAL", .token = 0xEA, .flags = kMiddle},
    {.text = "LEFT$(", .token = 0xC0, .flags = 0},
    {.text = "LEN", .token = 0xA9, .flags = 0},
    {.text = "LET", .token = 0xE9, .flags = kStart},
    {.text = "LOG", .token = 0xAB, .flags = 0},
    {.text = "LN", .token = 0xAA, .flags = 0},
    {.text = "MID$(", .token = 0xC1, .flags = 0},
    {.text = "MODE", .token = 0xEB, .flags = kMiddle},
    {.text = "MOD", .token = 0x83, .flags = 0},
    {.text = "MOVE", .token = 0xEC, .flags = kMiddle},
    {.text = "NEXT", .token = 0xED, .flags = kMiddle},
    {.text = "NEW", .token = 0xCA, .flags = kConditional},
    {.text = "NOT", .token = 0xAC, .flags = 0},
    {.text = "OLD", .token = 0xCB, .flags = kConditional},
    {.text = "ON", .token = 0xEE, .flags = kMiddle},
    {.text = "OFF", .token = 0x87, .flags = 0},
    {.text = "OR", .token = 0x84, .flags = 0},
    {.text = "OPENIN", .token = 0x8E, .flags = 0},
    {.text = "OPENOUT", .token = 0xAE, .flags = 0},
    {.text = "OPENUP", .token = 0xAD, .flags = 0},
    {.text = "OSCLI", .token = 0xFF, .flags = kMiddle},
    {.text = "PRINT", .token = 0xF1, .flags = kMiddle},
    {.text = "PAGE",
     .token = 0x90,
     .flags = kConditional | kMiddle | kPseudoVariable},
    {.text = "PTR",
     .token = 0x8F,
     .flags = kConditional | kMiddle | kPseudoVariable},
    {.text = "PI", .token = 0xAF, .flags = kConditional},
    {.text = "PLOT", .token = 0xF0, .flags = kMiddle},
    {.text = "POINT(", .token = 0xB0, .flags = 0},
    {.text = "PROC", .token = 0xF2, .flags = kMiddle | kFnProc},
    {.text = "POS", .token = 0xB1, .flags = kConditional},
    {.text = "RETURN", .token = 0xF8, .flags = kConditional},
    {.text = "REPEAT", .token = 0xF5, .flags = 0},
    {.text = "REPORT", .token = 0xF6, .flags = kConditional},
    {.text = "READ", .token = 0xF3, .flags = kMiddle},
    {.text = "REM", .token = 0xF4, .flags = kRestOfLine},
    {.text = "RUN", .token = 0xF9, .flags = kConditional},
    {.text = "RAD", .token = 0xB2, .flags = 0},
    {.text = "RESTORE", .token = 0xF7, .flags = kMiddle | kLineNumber},
    {.text = "RIGHT$(", .token = 0xC2, .flags = 0},
    {.text = "RND", .token = 0xB3, .flags = kConditional},
    {.text = "RENUMBER", .token = 0xCC, .flags = kLineNumber},
    {.text = "STEP", .token = 0x88, .flags = 0},
    {.text = "SAVE", .token = 0xCD, .flags = kMiddle},
    {.text = "SGN", .token = 0xB4, .flags = 0},
    {.text = "SIN", .token = 0xB5, .flags = 0},
    {.text = "SQR", .token = 0xB6, .flags = 0},
    {.text = "SPC", .token = 0x89, .flags = 0},
    {.text = "STR$", .token = 0xC3, .flags = 0},
    {.text = "STRING$(", .token = 0xC4, .flags = 0},
    {.text = "SOUND", .token = 0xD4, .flags = kMiddle},
    {.text = "STOP", .token = 0xFA, .flags = kConditional},
    {.text = "TAN", .token = 0xB7, .flags = 0},
    {.text = "THEN", .token = 0x8C, .flags = kStart | kLineNumber},
    {.text = "TO", .token = 0xB8, .flags = 0},
    {.text = "TAB(", .token = 0x8A, .flags = 0},
    {.text = "TRACE", .token = 0xFC, .flags = kMiddle | kLineNumber},
    {.text = "TIME",
     .token = 0x91,
     .flags = kConditional | kMiddle | kPseudoVariable},
    {.text = "TRUE", .token = 0xB9, .flags = kConditional},
    {.text = "UNTIL", .token = 0xFD, .flags = kMiddle},
    {.text = "USR", .token = 0xBA, .flags = 0},
    {.text = "VDU", .token = 0xEF, .flags = kMiddle},
    {.text = "VAL", .token = 0xBB, .flags = 0},
    {.text = "VPOS", .token = 0xBC, .flags = kConditional},
    {.text = "WIDTH", .token = 0xFE, .flags = kMiddle},
}};

bool IsDigit(char c) { return c >= '0' && c <= '9'; }
bool IsUpper(char c) { return c >= 'A' && c <= 'Z'; }
bool IsAlphanumeric(char c) {
  return IsDigit(c) || IsUpper(c) || (c >= 'a' && c <= 'z') || c == '_';
}
bool IsHexDigit(char c) { return IsDigit(c) || (c >= 'A' && c <= 'F'); }

// Tokenises one line's text, after its number, as BASIC II does.
class LineTokeniser {
 public:
  explicit LineTokeniser(std::string_view text) : text_(text) {}

  std::vector<std::uint8_t> Run() {
    while (at_ < text_.size()) {
      const char c = text_[at_];
      if (c == '"') {
        CopyString();
      } else if (c == ':') {
        Copy(1);
        start_ = true;
        line_numbers_ = false;
      } else if (c == ' ' || c == ',') {
        // Neither changes the state: GOTO 10, 20 has two line numbers.
        Copy(1);
      } else if (c == '*' && start_) {
        // A star command is passed to the operating system as typed.
        Copy(text_.size() - at_);
      } else if (c == '&') {
        Copy(1 + Span(at_ + 1, IsHexDigit));
        start_ = false;
      } else if (IsDigit(c) || c == '.') {
        Number();
      } else if (IsUpper(c) && TryKeyword()) {
        // Tokenised.
      } else if (IsAlphanumeric(c)) {
        // A variable or other name: none of it is tokenised.
        Copy(Span(at_, IsAlphanumeric));
        start_ = false;
        line_numbers_ = false;
      } else {
        Copy(1);
        start_ = false;
        line_numbers_ = false;
      }
    }
    return std::move(out_);
  }

 private:
  // The length of the run of characters from `from` that `in_run` accepts.
  std::size_t Span(std::size_t from, bool (*in_run)(char)) const {
    std::size_t end = from;
    while (end < text_.size() && in_run(text_[end])) {
      ++end;
    }
    return end - from;
  }

  void Copy(std::size_t count) {
    const std::string_view part = text_.substr(at_, count);
    out_.insert(out_.end(), part.begin(), part.end());
    at_ += count;
  }

  // A string, to its closing quote or the end of the line.
  void CopyString() {
    const std::size_t close = text_.find('"', at_ + 1);
    Copy(close == std::string_view::npos ? text_.size() - at_
                                         : close + 1 - at_);
  }

  void Number() {
    const std::size_t digits = Span(at_, IsDigit);
    if (line_numbers_ && digits > 0) {
      int number = 0;
      for (std::size_t i = 0;
           i < digits && number <= BasicTokeniser::kMaxLineNumber; ++i) {
        number = number * 10 + (text_[at_ + i] - '0');
      }
      if (number <= BasicTokeniser::kMaxLineNumber) {
        const std::array<std::uint8_t, 4> encoded =
            BasicTokeniser::EncodeLineNumber(number);
        out_.insert(out_.end(), encoded.begin(), encoded.end());
        at_ += digits;
        return;
      }
      // Too large for a line number: left as digits, and the numbers after
      // it are not line numbers either.
      Copy(digits);
      start_ = false;
      line_numbers_ = false;
      return;
    }
    Copy(Span(at_, [](char c) { return IsDigit(c) || c == '.'; }));
    start_ = false;
  }

  // Tokenises the keyword at the cursor, if there is one. A keyword
  // matches in full, or as a prefix of at least one character ended by
  // "." (P. for PRINT).
  bool TryKeyword() {
    for (const KeywordEntry& keyword : kKeywords) {
      std::size_t matched = 0;
      while (matched < keyword.text.size() && at_ + matched < text_.size() &&
             text_[at_ + matched] == keyword.text[matched]) {
        ++matched;
      }
      std::size_t length = 0;
      if (matched == keyword.text.size()) {
        length = matched;
      } else if (matched > 0 && at_ + matched < text_.size() &&
                 text_[at_ + matched] == '.') {
        length = matched + 1;
      } else {
        continue;
      }
      if ((keyword.flags & kConditional) != 0 && at_ + length < text_.size() &&
          IsAlphanumeric(text_[at_ + length])) {
        return false;
      }
      Emit(keyword, length);
      return true;
    }
    return false;
  }

  void Emit(const KeywordEntry& keyword, std::size_t length) {
    std::uint8_t token = keyword.token;
    if ((keyword.flags & kPseudoVariable) != 0 && start_) {
      token += kPseudoVariableStatement;
    }
    out_.push_back(token);
    at_ += length;
    if ((keyword.flags & kFnProc) != 0) {
      Copy(Span(at_, IsAlphanumeric));
    }
    if ((keyword.flags & kRestOfLine) != 0) {
      Copy(text_.size() - at_);
    }
    if ((keyword.flags & kMiddle) != 0) {
      start_ = false;
    }
    if ((keyword.flags & kStart) != 0) {
      start_ = true;
    }
    if ((keyword.flags & kLineNumber) != 0) {
      line_numbers_ = true;
    }
  }

  std::string_view text_;
  std::size_t at_ = 0;
  std::vector<std::uint8_t> out_;
  // At the start of a statement (pseudo-variables take their statement
  // tokens, and "*" starts a star command).
  bool start_ = true;
  // Numbers are line numbers.
  bool line_numbers_ = false;
};

}  // namespace

std::array<std::uint8_t, 4> BasicTokeniser::EncodeLineNumber(int number) {
  const auto low = static_cast<std::uint8_t>(number & 0xFF);
  const auto high = static_cast<std::uint8_t>((number >> 8) & 0xFF);
  // The top two bits of each byte, packed and flipped, then the low six
  // bits of each with bit 6 set, so no byte is a control code or a token.
  return {kLineNumberToken,
          static_cast<std::uint8_t>(
              (((low & 0xC0) >> 2) | ((high & 0xC0) >> 4)) ^ 0x54),
          static_cast<std::uint8_t>((low & 0x3F) | 0x40),
          static_cast<std::uint8_t>((high & 0x3F) | 0x40)};
}

StatusOr<std::vector<std::uint8_t>> BasicTokeniser::Tokenise(
    std::string_view source) {
  std::vector<std::uint8_t> program;
  int previous = -1;
  int text_line = 0;
  std::size_t at = 0;
  while (at < source.size()) {
    std::size_t end = source.find_first_of("\r\n", at);
    if (end == std::string_view::npos) {
      end = source.size();
    }
    const std::string_view line = source.substr(at, end - at);
    at = end + 1;
    if (end + 1 < source.size() && source[end] == '\r' &&
        source[end + 1] == '\n') {
      ++at;
    }
    ++text_line;
    if (line.find_first_not_of(' ') == std::string_view::npos) {
      continue;
    }
    for (const char c : line) {
      if (c < ' ' || c > '~') {
        return InvalidArgumentError(
            fmt::format("BASIC line {}: only printable ASCII characters can be "
                        "tokenised",
                        text_line));
      }
    }
    std::size_t digits = 0;
    int number = 0;
    while (digits < line.size() && IsDigit(line[digits])) {
      number = number * 10 + (line[digits] - '0');
      ++digits;
      if (number > kMaxLineNumber) {
        return InvalidArgumentError(
            fmt::format("BASIC line {}: line numbers go up to {}", text_line,
                        kMaxLineNumber));
      }
    }
    if (digits == 0) {
      return InvalidArgumentError(
          fmt::format("BASIC line {}: no line number", text_line));
    }
    if (number <= previous) {
      return InvalidArgumentError(
          fmt::format("BASIC line {}: line {} does not follow line {}",
                      text_line, number, previous));
    }
    const std::string_view text = line.substr(digits);
    if (text.find_first_not_of(' ') == std::string_view::npos) {
      return InvalidArgumentError(
          fmt::format("BASIC line {}: line {} has no text", text_line, number));
    }
    const std::vector<std::uint8_t> tokens = LineTokeniser(text).Run();
    if (tokens.size() + kLineOverhead > kMaxLineBytes) {
      return InvalidArgumentError(fmt::format(
          "BASIC line {}: line {} is too long once tokenised ({} bytes of "
          "{})",
          text_line, number, tokens.size() + kLineOverhead, kMaxLineBytes));
    }
    program.push_back(kCarriageReturn);
    program.push_back(static_cast<std::uint8_t>(number >> 8));
    program.push_back(static_cast<std::uint8_t>(number & 0xFF));
    program.push_back(static_cast<std::uint8_t>(tokens.size() + kLineOverhead));
    program.insert(program.end(), tokens.begin(), tokens.end());
    previous = number;
  }
  program.push_back(kCarriageReturn);
  program.push_back(kEndOfProgram);
  return program;
}

}  // namespace speak5220::core::bbc
