/*
 * File:        encoder_golden_test.cpp
 * Module:      encoder tests
 * Purpose:     Encodes the reference corpus and compares the frames with the
 *              Python reference encoder's (plan §3.2)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/encoder_golden.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

using core::encoder::EncoderOptions;

// Plan §3.2: identical frames on at least 99% of files, and an encoding
// that differs (at near-tie frames only) scores within this of the
// reference's overall quality score.
constexpr double kIdenticalShare = 0.99;
constexpr double kScoreTolerance = 0.02;

std::string Hex(const std::vector<std::uint8_t>& bytes) {
  constexpr std::string_view kDigits = "0123456789abcdef";
  std::string out;
  for (const std::uint8_t b : bytes) {
    out += kDigits[b >> 4];
    out += kDigits[b & 0xF];
  }
  return out;
}

std::string List(const std::vector<std::size_t>& values) {
  std::ostringstream out;
  for (std::size_t i = 0; i < values.size(); ++i) {
    out << (i ? "," : "") << values[i];
  }
  return values.empty() ? "-" : out.str();
}

struct CorpusTally {
  int files = 0;
  int identical = 0;
  int confined = 0;
  std::vector<std::string> failures;
};

// Encodes `inputs` (stem -> WAV path) with every option set and compares
// with the reference. Writes one report row per encoding to `report`, if
// open.
CorpusTally CheckCorpus(
    const std::map<std::string, std::filesystem::path>& inputs,
    std::ofstream* report) {
  CorpusTally tally;
  const auto sets = ReadOptionSets(EncoderGoldenDir() / "option_sets.tsv");
  EXPECT_TRUE(sets.IsOk()) << sets.GetStatus().ToString();
  if (!sets.IsOk()) {
    return tally;
  }
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth synth;
  core::quality::QualityAssessor quality(synth);
  for (const auto& [set_name, options] : *sets) {
    const auto refs = ReadReferenceEncodings(EncoderGoldenDir() / "frames" /
                                             (set_name + ".tsv"));
    EXPECT_TRUE(refs.IsOk()) << refs.GetStatus().ToString();
    if (!refs.IsOk()) {
      continue;
    }
    for (const auto& [stem, path] : inputs) {
      std::string trace = set_name;
      trace += ' ';
      trace += stem;
      SCOPED_TRACE(trace);
      const auto ref = refs->find(stem);
      EXPECT_NE(ref, refs->end()) << "no reference for " << stem;
      if (ref == refs->end()) {
        continue;
      }
      const auto wav = ReadWavSamples(path);
      EXPECT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
      if (!wav.IsOk()) {
        continue;
      }
      const auto result =
          encoder->Encode(wav->samples, wav->sample_rate, options);
      EXPECT_TRUE(result.IsOk()) << result.GetStatus().ToString();
      if (!result.IsOk()) {
        continue;
      }
      const EncodingComparison cmp =
          CompareEncoding(ref->second, result->frames);
      ++tally.files;
      tally.identical += cmp.identical ? 1 : 0;
      tally.confined += (!cmp.identical && cmp.confined_to_near_ties) ? 1 : 0;
      if (!cmp.identical && !cmp.confined_to_near_ties) {
        std::string failure = set_name;
        failure += '/';
        failure += stem;
        failure += " frames ";
        failure += List(cmp.differing);
        tally.failures.push_back(std::move(failure));
      }
      // A near-tie encoding must sound as good as the reference's.
      double score_difference = 0.0;
      if (!cmp.identical) {
        const auto mine = quality.Assess(wav->samples, result->frames);
        const auto theirs = quality.Assess(wav->samples, ref->second.frames);
        std::string failure = set_name;
        failure += '/';
        failure += stem;
        if (!mine.IsOk() || !theirs.IsOk()) {
          failure += " quality not measured";
          tally.failures.push_back(std::move(failure));
        } else {
          score_difference = mine->score.overall - theirs->score.overall;
          if (std::fabs(score_difference) > kScoreTolerance) {
            failure += " quality score differs by ";
            failure += std::to_string(score_difference);
            tally.failures.push_back(std::move(failure));
          }
        }
      }
      if (report != nullptr) {
        const auto packed = core::lpc::FrameCodec::Pack(result->frames);
        *report << set_name << '\t' << stem << '\t'
                << (cmp.identical
                        ? "identical"
                        : (cmp.confined_to_near_ties ? "near-tie" : "differs"))
                << '\t' << List(cmp.differing) << '\t' << score_difference
                << '\t' << (packed.IsOk() ? Hex(*packed) : "-") << '\n';
      }
    }
  }
  return tally;
}

void ExpectAccepted(const CorpusTally& tally) {
  ASSERT_GT(tally.files, 0);
  const double share =
      static_cast<double>(tally.identical) / static_cast<double>(tally.files);
  std::ostringstream failures;
  for (const std::string& f : tally.failures) {
    failures << "\n  " << f;
  }
  EXPECT_GE(share, kIdenticalShare)
      << tally.identical << " of " << tally.files << " identical";
  EXPECT_TRUE(tally.failures.empty())
      << "differences outside the reference's near-tie frames:"
      << failures.str();
}

// Plan Phase 2 task 6: the corpus subset (committed as WAV) encodes to the
// reference frames with every option set, except at near-tie frames, where
// it scores within kScoreTolerance of the reference's overall score.
TEST(EncoderGoldenTest, MatchesReferenceFramesOnCorpusSubset) {
  std::map<std::string, std::filesystem::path> inputs;
  for (const auto& entry :
       std::filesystem::directory_iterator(EncoderGoldenDir() / "inputs")) {
    if (entry.path().extension() == ".wav") {
      inputs[entry.path().stem().string()] = entry.path();
    }
  }
  ASSERT_FALSE(inputs.empty()) << "no inputs in " << EncoderGoldenDir();
  ExpectAccepted(CheckCorpus(inputs, nullptr));
}

// The full-corpus acceptance of plan §3.2, run by app/scripts/corpus-check.sh
// (TESTING.md), not by CTest: it reads the WAV copies of the whole corpus
// from SPEAK5220_CORPUS_DIR (app/scripts/generate_corpus.py) and writes a
// report, with each encoding's score difference from the reference's and
// its frames, to SPEAK5220_CORPUS_REPORT.
TEST(EncoderCorpusCheck, DISABLED_MatchesReferenceFramesOnFullCorpus) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* report_path = std::getenv("SPEAK5220_CORPUS_REPORT");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR (corpus-check.sh)";
  ASSERT_NE(report_path, nullptr)
      << "set SPEAK5220_CORPUS_REPORT (corpus-check.sh)";
  std::map<std::string, std::filesystem::path> inputs;
  for (const auto& entry : std::filesystem::directory_iterator(corpus)) {
    if (entry.path().extension() == ".wav") {
      inputs[entry.path().stem().string()] = entry.path();
    }
  }
  ASSERT_FALSE(inputs.empty()) << "no WAV files in " << corpus;
  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << "set\tname\tresult\tdiffering\tscore_difference\tpacked\n";
  ExpectAccepted(CheckCorpus(inputs, &report));
}

}  // namespace
}  // namespace speak5220::testing
