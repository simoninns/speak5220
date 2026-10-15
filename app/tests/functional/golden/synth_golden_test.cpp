/*
 * File:        synth_golden_test.cpp
 * Module:      synth tests
 * Purpose:     Compares Synth and the reference synthesiser with the C
 *              reference emulator's runs in app/golden/
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include "core/lpc/frame.h"
#include "core/synth/reference_synthesizer.h"
#include "core/synth/synth.h"
#include "gtest/gtest.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

using core::synth::ReferenceSynthesizer;
using core::synth::Synth;
using core::synth::SynthPath;
using core::synth::SynthResult;

const std::filesystem::path kEmulatorGolden =
    std::filesystem::path(SPEAK5220_GOLDEN_DIR) / "emulator";

// The phrase runs at power-up phase 0, the procedure Synth uses.
std::vector<std::map<std::string, std::string>> PhraseRuns() {
  const core::StatusOr<std::vector<std::map<std::string, std::string>>> runs =
      ReadTable(kEmulatorGolden / "runs.tsv");
  EXPECT_TRUE(runs.IsOk()) << runs.GetStatus().ToString();
  std::vector<std::map<std::string, std::string>> out;
  if (runs.IsOk()) {
    std::copy_if(runs->begin(), runs->end(), std::back_inserter(out),
                 [](const auto& row) {
                   return row.at("mode") != "script" && row.at("phase") == "0";
                 });
  }
  return out;
}

// Synth renders a phrase exactly as the reference emulator's bulk mode
// does: its codes are the reference run's from the first frame read.
TEST(SynthGoldenTest, MatchesReferenceEmulatorThroughBothPaths) {
  const auto runs = PhraseRuns();
  ASSERT_FALSE(runs.empty());
  Synth synth;
  for (const auto& row : runs) {
    const std::string& name = row.at("name");
    SCOPED_TRACE(name);
    const core::StatusOr<std::vector<std::uint8_t>> phrase =
        ReadBytes(kEmulatorGolden / row.at("input"));
    const core::StatusOr<std::vector<int>> codes =
        ReadCodes(kEmulatorGolden / "runs" / (name + ".codes"));
    ASSERT_TRUE(phrase.IsOk()) << phrase.GetStatus().ToString();
    ASSERT_TRUE(codes.IsOk()) << codes.GetStatus().ToString();
    const SynthPath path = row.at("mode") == "external"
                               ? SynthPath::kSpeakExternal
                               : SynthPath::kVsm;
    const core::StatusOr<SynthResult> result = synth.Synthesize(*phrase, path);
    if (!result.IsOk()) {
      // A Speak External stream under 9 bytes never starts (§12.9); the
      // reference run then stays idle throughout.
      EXPECT_EQ(path, SynthPath::kSpeakExternal);
      EXPECT_TRUE(std::all_of(codes->begin(), codes->end(),
                              [](int c) { return c == -1; }));
      continue;
    }
    const auto first = static_cast<std::size_t>(result->first_period);
    ASSERT_EQ(first + result->codes.size(), codes->size());
    EXPECT_TRUE(
        std::equal(result->codes.begin(), result->codes.end(),
                   codes->begin() + static_cast<std::ptrdiff_t>(first)));
  }
}

// The encoder's reference-algorithm synthesiser matches
// the emulator's VSM path, from the first frame read (sample 200), on the
// reference frame lists.
TEST(SynthGoldenTest, ReferenceSynthesizerMatchesEmulatorVsmPath) {
  int compared = 0;
  for (const auto& row : PhraseRuns()) {
    if (row.at("mode") != "vsm") {
      continue;
    }
    const std::string& name = row.at("name");
    SCOPED_TRACE(name);
    EXPECT_EQ(row.at("synth_matches"), "yes")
        << "the Python reference did not match the emulator here";
    std::filesystem::path csv = kEmulatorGolden / row.at("input");
    csv.replace_extension(".csv");
    const core::StatusOr<std::vector<core::lpc::Frame>> frames =
        ReadFramesCsv(csv);
    const core::StatusOr<std::vector<int>> codes =
        ReadCodes(kEmulatorGolden / "runs" / (name + ".codes"));
    ASSERT_TRUE(frames.IsOk()) << frames.GetStatus().ToString();
    ASSERT_TRUE(codes.IsOk()) << codes.GetStatus().ToString();
    const std::vector<int> synthesized =
        ReferenceSynthesizer::Synthesize(*frames).codes;
    constexpr std::size_t kFirstFrameRead = 200;
    ASSERT_LE(kFirstFrameRead + synthesized.size(), codes->size());
    EXPECT_TRUE(std::equal(synthesized.begin(), synthesized.end(),
                           codes->begin() + kFirstFrameRead));
    ++compared;
  }
  EXPECT_GT(compared, 0);
}

}  // namespace
}  // namespace speak5220::testing
