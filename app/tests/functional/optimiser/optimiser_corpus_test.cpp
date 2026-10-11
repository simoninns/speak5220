/*
 * File:        optimiser_corpus_test.cpp
 * Module:      optimiser tests
 * Purpose:     Optimises every corpus file from the default options and
 *              checks that PESQ rises, within a per-phrase byte limit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>

#include "core/encoder/encoder.h"
#include "core/lpc/frame_statistics.h"
#include "core/optimiser/optimiser.h"
#include "core/quality/pesq_metric.h"
#include "core/synth/synth.h"
#include "core/util/task_runner.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

namespace optimiser = core::optimiser;

constexpr double kSampleRate = 8000.0;

// Run by app/scripts/corpus-check.sh, not by CTest (plan Phase 7, item 4):
// optimises every WAV file in SPEAK5220_CORPUS_DIR from the default
// options on every core, with the per-phrase byte limit set to the
// default encoding's size, so a better score cannot come from more bytes.
// SPEAK5220_OPTIMISE_STRIDE=N takes every Nth file. Writes one row per
// file to SPEAK5220_OPTIMISE_REPORT. Passes if the best is within the
// limit on every file and scores above the defaults on most of them.
TEST(OptimiserCorpusCheck, DISABLED_RaisesPesqOverTheDefaults) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* report_path = std::getenv("SPEAK5220_OPTIMISE_REPORT");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR (corpus-check.sh)";
  ASSERT_NE(report_path, nullptr)
      << "set SPEAK5220_OPTIMISE_REPORT (corpus-check.sh)";
  int stride = 1;
  if (const char* text = std::getenv("SPEAK5220_OPTIMISE_STRIDE")) {
    stride = std::max(1, std::atoi(text));
  }
  std::map<std::string, std::filesystem::path> inputs;
  for (const auto& entry : std::filesystem::directory_iterator(corpus)) {
    if (entry.path().extension() == ".wav") {
      inputs[entry.path().stem().string()] = entry.path();
    }
  }
  ASSERT_FALSE(inputs.empty()) << "no WAV files in " << corpus;
  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << "name\tspeech_seconds\tdefault_pesq\tdefault_bytes\tbest_pesq\t"
            "best_bytes\tcandidates\tseconds\n";

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::quality::PesqMetric metric(synth);
  core::ThreadPoolTaskRunner workers;
  optimiser::Optimiser search(*encoder, metric, workers);

  int files = 0;
  int raised = 0;
  double gain = 0.0;
  double speech = 0.0;
  double took = 0.0;
  int index = 0;
  for (const auto& [name, path] : inputs) {
    if (index++ % stride != 0) {
      continue;
    }
    const auto wav = ReadWavSamples(path);
    ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    const auto defaults = encoder->Encode(wav->samples, wav->sample_rate, {});
    ASSERT_TRUE(defaults.IsOk()) << name;
    const auto default_bytes = core::lpc::SummariseFrames(defaults->frames);
    const auto start = std::chrono::steady_clock::now();
    const auto result =
        search.Run({.source = wav->samples,
                    .start = {},
                    .limits = {.max_bytes = default_bytes.bytes},
                    .budget = {},
                    .plan = {}});
    const std::chrono::duration<double> seconds =
        std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(result.IsOk()) << name << ": " << result.GetStatus().ToString();
    const optimiser::Candidate* best = result->Best();
    ASSERT_NE(best, nullptr) << name;
    EXPECT_LE(best->bytes, default_bytes.bytes) << name;
    const double default_pesq = result->Start()->score;
    ++files;
    raised += best->score > default_pesq ? 1 : 0;
    gain += best->score - default_pesq;
    const double length =
        static_cast<double>(wav->samples.size()) / kSampleRate;
    speech += length;
    took += seconds.count();
    report << name << '\t' << length << '\t' << default_pesq << '\t'
           << default_bytes.bytes << '\t' << best->score << '\t' << best->bytes
           << '\t' << result->history.size() << '\t' << seconds.count() << '\n';
  }
  std::cout << "optimise: " << files << " files; PESQ raised on " << raised
            << ", mean gain " << gain / files << "; " << took / speech
            << " s per second of speech on " << workers.ThreadCount()
            << " threads\n";
  EXPECT_GT(2 * raised, files)
      << "PESQ rose on only " << raised << " of " << files << " files";
}

}  // namespace
}  // namespace speak5220::testing
