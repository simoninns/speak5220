/*
 * File:        encoder_benchmark_test.cpp
 * Module:      encoder tests
 * Purpose:     Times the encoder on a set of WAV files, for
 *              app/scripts/benchmark.sh
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
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

// The target: under 0.05 s per second of speech on one core.
constexpr double kTargetSecondsPerSecond = 0.05;
// Each file is encoded this many times, and the fastest run counts, so
// other load on the machine affects the result less.
constexpr int kRuns = 3;
constexpr double kSampleRate = 8000.0;

// Run by app/scripts/benchmark.sh, not by CTest: it times the default
// encoding of every WAV file in SPEAK5220_BENCHMARK_DIR (the corpus
// subset in app/golden/encoder/inputs/ if unset), one at a time on one
// core, and writes one row per file to SPEAK5220_BENCHMARK_REPORT, if set.
TEST(EncoderBenchmark, DISABLED_EncodesFasterThanTarget) {
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
    report << "name\tspeech_seconds\tencode_seconds\n";
  }

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  double speech = 0.0;
  double spent = 0.0;
  for (const auto& [name, path] : inputs) {
    const auto wav = ReadWavSamples(path);
    ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    double best = 0.0;
    for (int run = 0; run < kRuns; ++run) {
      const auto start = std::chrono::steady_clock::now();
      const auto result = encoder->Encode(wav->samples, wav->sample_rate, {});
      const std::chrono::duration<double> took =
          std::chrono::steady_clock::now() - start;
      ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
      best = run == 0 ? took.count() : std::min(best, took.count());
    }
    const double seconds =
        static_cast<double>(wav->samples.size()) / kSampleRate;
    speech += seconds;
    spent += best;
    if (report.is_open()) {
      report << name << '\t' << seconds << '\t' << best << '\n';
    }
  }
  const double rate = spent / speech;
  std::cout << "encoder: " << inputs.size() << " files, " << speech
            << " s of speech in " << spent << " s: " << rate
            << " s per second of speech (target " << kTargetSecondsPerSecond
            << ")\n";
  EXPECT_LT(rate, kTargetSecondsPerSecond);
}

}  // namespace
}  // namespace speak5220::testing
