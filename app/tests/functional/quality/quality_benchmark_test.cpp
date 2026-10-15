/*
 * File:        quality_benchmark_test.cpp
 * Module:      quality tests
 * Purpose:     Times the quality measures on the corpus subset, one file at
 *              a time on one core
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/synth.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

// Each measurement is repeated this many times and the fastest counts.
constexpr int kRuns = 3;
constexpr double kSampleRate = 8000.0;

// The fastest of kRuns calls of `f`, in seconds.
double Fastest(const std::function<void()>& f) {
  double best = 0.0;
  for (int run = 0; run < kRuns; ++run) {
    const auto start = std::chrono::steady_clock::now();
    f();
    const std::chrono::duration<double> took =
        std::chrono::steady_clock::now() - start;
    best = run == 0 ? took.count() : std::min(best, took.count());
  }
  return best;
}

// Run by app/scripts/benchmark.sh, not by CTest: for the default encoding
// of every WAV file in SPEAK5220_BENCHMARK_DIR (the corpus subset if
// unset), times QualityAssessor (the synthesis, alignment, preparation,
// the three measures with REAPER, and the score), one file at a time on
// one core. Writes one row per file to SPEAK5220_BENCHMARK_REPORT, if set.
// There is no target.
TEST(QualityBenchmark, DISABLED_TimesTheQualityMeasures) {
  const char* dir_env = std::getenv("SPEAK5220_BENCHMARK_DIR");
  const std::filesystem::path dir = dir_env != nullptr
                                        ? std::filesystem::path(dir_env)
                                        : EncoderGoldenDir() / "inputs";
  std::map<std::string, std::filesystem::path> inputs;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".wav") {
      inputs[entry.path().stem().string()] = entry.path();
    }
  }
  ASSERT_FALSE(inputs.empty()) << "no WAV files in " << dir;
  std::ofstream report;
  if (const char* path = std::getenv("SPEAK5220_BENCHMARK_REPORT")) {
    report.open(path);
    ASSERT_TRUE(report.is_open()) << "cannot write " << path;
    report << "name\tspeech_seconds\tquality_seconds\n";
  }

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  double speech = 0.0;
  double total = 0.0;
  for (const auto& [name, path] : inputs) {
    const auto wav = ReadWavSamples(path);
    ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    const auto encoding = encoder->Encode(wav->samples, wav->sample_rate, {});
    ASSERT_TRUE(encoding.IsOk());
    // A fresh synth per run, so its cache does not hide the synthesis.
    const double took = Fastest([&] {
      core::synth::Synth synth;
      core::quality::QualityAssessor assessor(synth);
      ASSERT_TRUE(assessor.Assess(wav->samples, encoding->frames).IsOk());
    });
    const double seconds =
        static_cast<double>(wav->samples.size()) / kSampleRate;
    speech += seconds;
    total += took;
    if (report.is_open()) {
      report << name << '\t' << seconds << '\t' << took << '\n';
    }
  }
  std::cout << "quality: " << inputs.size() << " files, " << speech
            << " s of speech; " << total / speech
            << " s per second of speech\n";
}

}  // namespace
}  // namespace speak5220::testing
