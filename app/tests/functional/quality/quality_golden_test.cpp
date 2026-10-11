/*
 * File:        quality_golden_test.cpp
 * Module:      quality tests
 * Purpose:     Measures the reference encodings with PESQ, the anchor and
 *              the cheap measures, and compares them with the reference
 *              tools' values (plan Phase 3)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/quality_golden.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/quality/anchor_vocoder.h"
#include "core/quality/cheap_measures.h"
#include "core/quality/pesq_metric.h"
#include "core/quality/quality_scale.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

using core::quality::QualityBand;
using core::quality::QualityScale;

// Plan Phase 3: PESQ within 0.01 of the reference's, the anchor within
// 0.02. The cheap measures come from the same arithmetic as the
// reference, on frames and intermediates that match it.
constexpr double kPesqTolerance = 0.01;
constexpr double kAnchorTolerance = 0.02;
constexpr double kCheapTolerance = 1e-6;

// The WAV files in `dir`, by stem.
std::map<std::string, std::filesystem::path> WavFiles(
    const std::filesystem::path& dir) {
  std::map<std::string, std::filesystem::path> inputs;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".wav") {
      inputs[entry.path().stem().string()] = entry.path();
    }
  }
  return inputs;
}

bool Within(double a, double b, double tolerance) {
  if (std::isnan(a) || std::isnan(b)) {
    return std::isnan(a) && std::isnan(b);
  }
  return std::fabs(a - b) <= tolerance;
}

struct QualityTally {
  int encodings = 0;
  int anchors = 0;
  double worst_pesq = 0.0;
  double worst_anchor = 0.0;
  int lag_differences = 0;
  std::vector<std::string> failures;
};

void Fail(QualityTally& tally, const std::string& what) {
  tally.failures.push_back(what);
}

// Measures every input of `inputs` with every option set: PESQ of the
// reference frames, the cheap measures of the C++ encoder's encoding, and
// each input's anchor. Writes one report row per measurement to `report`,
// if open.
QualityTally CheckQuality(
    const std::map<std::string, std::filesystem::path>& inputs,
    std::ofstream* report) {
  QualityTally tally;
  const auto sets = ReadOptionSets(EncoderGoldenDir() / "option_sets.tsv");
  const auto anchors = ReadAnchorScores(QualityGoldenDir() / "anchor.tsv");
  EXPECT_TRUE(sets.IsOk()) << sets.GetStatus().ToString();
  EXPECT_TRUE(anchors.IsOk()) << anchors.GetStatus().ToString();
  if (!sets.IsOk() || !anchors.IsOk()) {
    return tally;
  }
  core::synth::Synth synth;
  core::quality::PesqMetric pesq(synth);
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();

  std::map<std::string, std::vector<double>> sources;
  for (const auto& [stem, path] : inputs) {
    const auto wav = ReadWavSamples(path);
    EXPECT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    if (wav.IsOk()) {
      sources[stem] = wav->samples;
    }
  }

  for (const auto& [set_name, options] : *sets) {
    const auto frames = ReadReferenceEncodings(EncoderGoldenDir() / "frames" /
                                               (set_name + ".tsv"));
    const auto refs =
        ReadQualityScores(QualityGoldenDir() / "scores" / (set_name + ".tsv"));
    EXPECT_TRUE(frames.IsOk()) << frames.GetStatus().ToString();
    EXPECT_TRUE(refs.IsOk()) << refs.GetStatus().ToString();
    if (!frames.IsOk() || !refs.IsOk()) {
      continue;
    }
    for (const auto& [stem, source] : sources) {
      std::string id = set_name;
      id += '/';
      id += stem;
      const auto ref = refs->find(stem);
      const auto enc = frames->find(stem);
      if (ref == refs->end() || enc == frames->end()) {
        Fail(tally, id + ": no reference");
        continue;
      }
      const auto m = pesq.MeasureFrames(source, enc->second.frames);
      const auto encoding = encoder->Encode(source, 8000, options);
      if (!m.IsOk() || !encoding.IsOk() || !encoding->details.has_value()) {
        Fail(tally, id + ": not measured");
        continue;
      }
      const core::quality::CheapMeasures cheap =
          core::quality::MeasureCheap(*encoding->details, encoding->frames);
      ++tally.encodings;
      const double pesq_diff = std::fabs(m->score - ref->second.pesq);
      tally.worst_pesq = std::max(tally.worst_pesq, pesq_diff);
      if (pesq_diff > kPesqTolerance) {
        Fail(tally, id + ": PESQ " + std::to_string(m->score) + ", reference " +
                        std::to_string(ref->second.pesq));
      }
      if (m->lag != ref->second.lag ||
          std::cmp_not_equal(m->output.size(), ref->second.samples)) {
        ++tally.lag_differences;
      }
      const bool cheap_ok =
          Within(cheap.level_error, ref->second.level_err, kCheapTolerance) &&
          Within(cheap.level_bias, ref->second.level_bias, kCheapTolerance) &&
          Within(cheap.level_p90, ref->second.level_p90, kCheapTolerance) &&
          Within(cheap.spectral_distance, ref->second.spec_dist,
                 kCheapTolerance);
      if (!cheap_ok) {
        std::ostringstream what;
        what << id << ": cheap measures " << cheap.level_error << ' '
             << cheap.level_bias << ' ' << cheap.level_p90 << ' '
             << cheap.spectral_distance << ", reference "
             << ref->second.level_err << ' ' << ref->second.level_bias << ' '
             << ref->second.level_p90 << ' ' << ref->second.spec_dist;
        Fail(tally, what.str());
      }
      if (report != nullptr) {
        *report << set_name << '\t' << stem << '\t' << m->lag << '\t'
                << m->output.size() << '\t' << m->score << '\t'
                << ref->second.pesq << '\t' << cheap.level_error << '\t'
                << cheap.spectral_distance << '\n';
      }
    }
  }

  for (const auto& [stem, source] : sources) {
    const auto ref = anchors->find(stem);
    const auto score = core::quality::AnchorScore(source);
    if (ref == anchors->end() || !score.IsOk()) {
      Fail(tally, "anchor/" + stem + ": not measured");
      continue;
    }
    ++tally.anchors;
    const double diff = std::fabs(*score - ref->second);
    tally.worst_anchor = std::max(tally.worst_anchor, diff);
    if (diff > kAnchorTolerance) {
      Fail(tally, "anchor/" + stem + ": " + std::to_string(*score) +
                      ", reference " + std::to_string(ref->second));
    }
    if (report != nullptr) {
      *report << "anchor\t" << stem << "\t-\t-\t" << *score << '\t'
              << ref->second << "\t-\t-\n";
    }
  }
  return tally;
}

void ExpectAccepted(const QualityTally& tally, std::size_t inputs) {
  ASSERT_GT(tally.encodings, 0);
  EXPECT_EQ(static_cast<std::size_t>(tally.anchors), inputs);
  std::ostringstream failures;
  for (const std::string& f : tally.failures) {
    failures << "\n  " << f;
  }
  EXPECT_TRUE(tally.failures.empty()) << failures.str();
  std::cout << "quality: " << tally.encodings << " encodings, worst PESQ "
            << "difference " << tally.worst_pesq << "; " << tally.anchors
            << " anchors, worst difference " << tally.worst_anchor << "; "
            << tally.lag_differences << " lags differ\n";
}

// Plan Phase 3 tasks 2-4: on the corpus subset (committed as WAV), PESQ,
// the anchor and the cheap measures match the reference tools.
TEST(QualityGoldenTest, MatchesReferenceOnCorpusSubset) {
  const auto inputs = WavFiles(EncoderGoldenDir() / "inputs");
  ASSERT_FALSE(inputs.empty()) << "no inputs in " << EncoderGoldenDir();
  const QualityTally tally = CheckQuality(inputs, nullptr);
  EXPECT_EQ(tally.lag_differences, 0);
  ExpectAccepted(tally, inputs.size());
}

// Plan Phase 3 task 4: the band thresholds place the encoder's typical
// results, the reference encodings of the whole corpus with the default
// options, in the upper bands.
TEST(QualityGoldenTest, CalibrationPlacesTypicalEncodingsInUpperBands) {
  const auto refs =
      ReadQualityScores(QualityGoldenDir() / "scores" / "default.tsv");
  const auto anchors = ReadAnchorScores(QualityGoldenDir() / "anchor.tsv");
  ASSERT_TRUE(refs.IsOk()) << refs.GetStatus().ToString();
  ASSERT_TRUE(anchors.IsOk()) << anchors.GetStatus().ToString();
  std::vector<double> positions;
  for (const auto& [stem, ref] : *refs) {
    positions.push_back(
        QualityScale::Place(ref.pesq, anchors->at(stem)).position);
  }
  ASSERT_EQ(positions.size(), anchors->size());
  const double median = core::dsp::Median(positions);
  EXPECT_GE(QualityScale::BandOf(median), QualityBand::kGood)
      << "median position " << median;
  const auto fair_or_better = std::count_if(
      positions.begin(), positions.end(),
      [](double p) { return QualityScale::BandOf(p) >= QualityBand::kFair; });
  EXPECT_GE(static_cast<double>(fair_or_better),
            0.75 * static_cast<double>(positions.size()));
}

// The full-corpus acceptance of Phase 3, run by app/scripts/corpus-check.sh
// (TESTING.md), not by CTest: it reads the WAV copies of the whole corpus
// from SPEAK5220_CORPUS_DIR and writes a report to
// SPEAK5220_QUALITY_REPORT.
TEST(QualityCorpusCheck, DISABLED_MatchesReferenceOnFullCorpus) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* report_path = std::getenv("SPEAK5220_QUALITY_REPORT");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR (corpus-check.sh)";
  ASSERT_NE(report_path, nullptr)
      << "set SPEAK5220_QUALITY_REPORT (corpus-check.sh)";
  const auto inputs = WavFiles(corpus);
  ASSERT_FALSE(inputs.empty()) << "no WAV files in " << corpus;
  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << "set\tname\tlag\tsamples\tpesq\treference\tlevel_err\t"
            "spec_dist\n";
  ExpectAccepted(CheckQuality(inputs, &report), inputs.size());
}

}  // namespace
}  // namespace speak5220::testing
