/*
 * File:        sha256_test.cpp
 * Module:      util tests
 * Purpose:     Unit tests of the SHA-256 digests
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/sha256.h"

#include <cstdint>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

// FIPS 180-2, Appendix B.1: the digest of "abc".
constexpr const char* kAbcDigest =
    "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";

TEST(Sha256Test, MatchesTheStandardsExample) {
  const std::vector<std::uint8_t> abc = {'a', 'b', 'c'};
  EXPECT_EQ(Sha256Hex(abc), kAbcDigest);
}

TEST(Sha256Test, IncrementalDigestEqualsTheWhole) {
  Sha256 hash;
  hash.Add("a");
  hash.Add(std::vector<std::uint8_t>{'b', 'c'});
  EXPECT_EQ(hash.Hex(), kAbcDigest);
}

TEST(Sha256Test, EmptyInputHasTheKnownDigest) {
  EXPECT_EQ(Sha256Hex({}),
            "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

}  // namespace
}  // namespace speak5220::core
