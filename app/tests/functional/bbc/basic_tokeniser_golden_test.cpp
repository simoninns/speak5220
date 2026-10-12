/*
 * File:        basic_tokeniser_golden_test.cpp
 * Module:      bbc tests
 * Purpose:     Checks the BBC BASIC II tokeniser against the reference
 *              programs as BASIC II itself tokenised them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/bbc/basic_tokeniser.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::bbc {
namespace {

namespace fs = std::filesystem;

std::string ReadFile(const fs::path& path) {
  std::ifstream stream(path, std::ios::binary);
  return {std::istreambuf_iterator<char>(stream),
          std::istreambuf_iterator<char>()};
}

// app/golden/bbc/programs/NAME.bas, and NAME.bin: the program BBC BASIC II
// stored when NAME.bas was typed in (app/scripts/golden/basic_programs.py).
TEST(BasicTokeniserGoldenTest, MatchesBasicTwoOnEveryReferenceProgram) {
  const fs::path directory = fs::path(SPEAK5220_GOLDEN_DIR) / "bbc/programs";
  int programs = 0;
  for (const fs::directory_entry& entry : fs::directory_iterator(directory)) {
    if (entry.path().extension() != ".bas") {
      continue;
    }
    ++programs;
    fs::path reference = entry.path();
    reference.replace_extension(".bin");
    const std::string expected = ReadFile(reference);
    const StatusOr<std::vector<std::uint8_t>> tokenised =
        BasicTokeniser::Tokenise(ReadFile(entry.path()));
    ASSERT_TRUE(tokenised.IsOk())
        << entry.path() << ": " << tokenised.GetStatus().ToString();
    EXPECT_EQ(std::string(tokenised->begin(), tokenised->end()), expected)
        << entry.path();
  }
  EXPECT_EQ(programs, 3);
}

}  // namespace
}  // namespace speak5220::core::bbc
