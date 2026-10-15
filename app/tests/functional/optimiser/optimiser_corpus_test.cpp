/*
 * File:        optimiser_corpus_test.cpp
 * Module:      optimiser tests
 * Purpose:     Optimises every corpus file from the default options and
 *              checks that the quality score rises, within a per-phrase
 *              byte limit; and scores any options over the corpus, for
 *              choosing the defaults
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
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame_statistics.h"
#include "core/optimiser/optimiser.h"
#include "core/preprocess/preprocess.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/synth.h"
#include "core/util/task_runner.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

namespace optimiser = core::optimiser;

constexpr double kSampleRate = 8000.0;

// The corpus's WAV files by name, from SPEAK5220_CORPUS_DIR.
std::map<std::string, std::filesystem::path> CorpusFiles(const char* corpus) {
  std::map<std::string, std::filesystem::path> inputs;
  for (const auto& entry : std::filesystem::directory_iterator(corpus)) {
    if (entry.path().extension() == ".wav") {
      inputs[entry.path().stem().string()] = entry.path();
    }
  }
  return inputs;
}

// Options from text such as "target_rms=18,noise_gate_db=30" over the
// defaults; empty or unset text gives the defaults.
core::encoder::EncoderOptions OptionsFromText(const char* text) {
  const core::StatusOr<core::encoder::EncoderOptions> options =
      EncoderOptionsFromText(text == nullptr ? "" : text);
  EXPECT_TRUE(options.IsOk()) << options.GetStatus().ToString();
  return options.IsOk() ? *options : core::encoder::EncoderOptions{};
}

// The options' fields as report columns: the header, then each value.
std::string OptionsHeader() {
  std::string header;
  for (const auto& info : core::encoder::EncoderOptionInfos()) {
    header += '\t';
    header += info.field;
  }
  return header;
}

std::string OptionsColumns(const core::encoder::EncoderOptions& options) {
  std::string columns;
  for (const auto& info : core::encoder::EncoderOptionInfos()) {
    columns += '\t';
    columns += core::encoder::EncoderOptionText(options, info.field);
  }
  return columns;
}

// Run by app/scripts/corpus-check.sh, not by CTest:
// optimises every WAV file in SPEAK5220_CORPUS_DIR from the default
// options on every core, with the per-phrase byte limit set to the
// default encoding's size, so a better score cannot come from more bytes.
// SPEAK5220_OPTIMISE_STRIDE=N takes every Nth file, and
// SPEAK5220_OPTIMISE_START (as "noise_gate_db=30,...") changes the start
// options; the limit stays at the size of the default encoding. Writes one
// row per file to SPEAK5220_OPTIMISE_REPORT, with the best options and
// the best's score against the source without a noise gate. Passes if the
// best is within the limit on every file and scores above the start on
// most of them.
TEST(OptimiserCorpusCheck, DISABLED_RaisesTheScoreOverTheDefaults) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* report_path = std::getenv("SPEAK5220_OPTIMISE_REPORT");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR (corpus-check.sh)";
  ASSERT_NE(report_path, nullptr)
      << "set SPEAK5220_OPTIMISE_REPORT (corpus-check.sh)";
  int stride = 1;
  if (const char* text = std::getenv("SPEAK5220_OPTIMISE_STRIDE")) {
    stride = std::max(1, std::atoi(text));
  }
  const core::encoder::EncoderOptions start_options =
      OptionsFromText(std::getenv("SPEAK5220_OPTIMISE_START"));
  const std::map<std::string, std::filesystem::path> inputs =
      CorpusFiles(corpus);
  ASSERT_FALSE(inputs.empty()) << "no WAV files in " << corpus;
  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << "name\tspeech_seconds\tdefault_score\tdefault_bytes\tbest_score\t"
            "best_bytes\tcandidates\tseconds\tbest_plain_score"
         << OptionsHeader() << '\n';

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::quality::QualityAssessor assessor(synth);
  core::quality::OverallQualityMetric metric(assessor);
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
                    .start = start_options,
                    .limits = {.max_bytes = default_bytes.bytes},
                    .budget = {},
                    .plan = {}});
    const std::chrono::duration<double> seconds =
        std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(result.IsOk()) << name << ": " << result.GetStatus().ToString();
    const optimiser::Candidate* best = result->Best();
    ASSERT_NE(best, nullptr) << name;
    EXPECT_LE(best->bytes, default_bytes.bytes) << name;
    const double default_score = result->Start()->score;
    ++files;
    raised += best->score > default_score ? 1 : 0;
    gain += best->score - default_score;
    const double length =
        static_cast<double>(wav->samples.size()) / kSampleRate;
    speech += length;
    took += seconds.count();
    report << name << '\t' << length << '\t' << default_score << '\t'
           << default_bytes.bytes << '\t' << best->score << '\t' << best->bytes
           << '\t' << result->history.size() << '\t' << seconds.count();
    // The best scored against the source as recorded, so runs with and
    // without a noise gate compare.
    const auto plain = assessor.Assess(wav->samples, result->best_frames);
    ASSERT_TRUE(plain.IsOk()) << name << ": " << plain.GetStatus().ToString();
    report << '\t' << plain->score.overall << OptionsColumns(best->options)
           << '\n';
  }
  std::cout << "optimise: " << files << " files; score raised on " << raised
            << ", mean gain " << gain / files << "; " << took / speech
            << " s per second of speech on " << workers.ThreadCount()
            << " threads\n";
  EXPECT_GT(2 * raised, files)
      << "the score rose on only " << raised << " of " << files << " files";
}

// Run by hand when choosing the default options:
// encodes every WAV file in SPEAK5220_CORPUS_DIR with the options in
// SPEAK5220_OPTIONS (as "target_rms=18,noise_gate_db=30"; unset for the
// defaults) and writes each one's size, encoding time and overall score,
// against its gated source and the source as recorded, to
// SPEAK5220_OPTIONS_REPORT.
TEST(OptionsCorpusCheck, DISABLED_ScoresTheOptions) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* report_path = std::getenv("SPEAK5220_OPTIONS_REPORT");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR";
  ASSERT_NE(report_path, nullptr) << "set SPEAK5220_OPTIONS_REPORT";
  const core::encoder::EncoderOptions options =
      OptionsFromText(std::getenv("SPEAK5220_OPTIONS"));
  const std::map<std::string, std::filesystem::path> inputs =
      CorpusFiles(corpus);
  ASSERT_FALSE(inputs.empty()) << "no WAV files in " << corpus;
  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << "name\tspeech_seconds\tbytes\tencode_seconds\tscore\tplain_"
            "score\n";

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::quality::QualityAssessor assessor(synth);
  for (const auto& [name, path] : inputs) {
    const auto wav = ReadWavSamples(path);
    ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    const auto start = std::chrono::steady_clock::now();
    const auto encoding =
        encoder->Encode(wav->samples, wav->sample_rate, options);
    const std::chrono::duration<double> encode_time =
        std::chrono::steady_clock::now() - start;
    ASSERT_TRUE(encoding.IsOk()) << name;
    const std::vector<double> source =
        core::preprocess::Preprocess(wav->samples, options.preprocess).samples;
    const auto scored = assessor.Assess(source, encoding->frames);
    ASSERT_TRUE(scored.IsOk()) << name << ": " << scored.GetStatus().ToString();
    const auto plain = assessor.Assess(wav->samples, encoding->frames);
    ASSERT_TRUE(plain.IsOk()) << name << ": " << plain.GetStatus().ToString();
    report << name << '\t'
           << static_cast<double>(wav->samples.size()) / kSampleRate << '\t'
           << core::lpc::SummariseFrames(encoding->frames).bytes << '\t'
           << encode_time.count() << '\t' << scored->score.overall << '\t'
           << plain->score.overall << '\n';
  }
}

}  // namespace
}  // namespace speak5220::testing
