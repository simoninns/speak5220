/*
 * File:        optimiser_benchmark_test.cpp
 * Module:      optimiser tests
 * Purpose:     Times Optimise on the benchmark project's phrases, on one
 *              thread and on every core
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QThread>
#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/encoder/encoder.h"
#include "core/optimiser/optimiser.h"
#include "core/project/directory_project_store.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_serializer.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/synth.h"
#include "core/util/task_runner.h"
#include "gtest/gtest.h"

namespace speak5220::testing {
namespace {

namespace optimiser = core::optimiser;
namespace project = core::project;

// Run by app/scripts/benchmark.sh, not by CTest (plan Phase 7, item 4):
// optimises every phrase of the project in SPEAK5220_BENCHMARK_PROJECT
// (app/projects/benchmark/synthetic-speech if unset) from its settings,
// within the project's budget, once on one thread and once on every core.
// Writes one row per phrase and thread count to
// SPEAK5220_BENCHMARK_REPORT, if set. The plan sets no target; the rates
// are recorded in the HLD.
TEST(OptimiserBenchmark, DISABLED_TimesOptimise) {
  const char* project_env = std::getenv("SPEAK5220_BENCHMARK_PROJECT");
  const std::filesystem::path path =
      project_env != nullptr ? std::filesystem::path(project_env)
                             : std::filesystem::path(SPEAK5220_PROJECTS_DIR) /
                                   "benchmark/synthetic-speech";
  core::audio::SndfileWavCodec codec;
  project::ProjectSerializer serializer(codec);
  project::DirectoryProjectStore store(path);
  const auto loaded = serializer.Load(store);
  ASSERT_TRUE(loaded.IsOk()) << path << ": " << loaded.GetStatus().ToString();
  ASSERT_FALSE(loaded->Phrases().empty()) << "no phrases in " << path;
  std::ofstream report;
  if (const char* report_path = std::getenv("SPEAK5220_BENCHMARK_REPORT")) {
    report.open(report_path);
    ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
    report << "threads\tname\tspeech_seconds\tcandidates\tseconds\t"
              "start_score\tbest_score\n";
  }

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::quality::QualityAssessor assessor(synth);
  core::quality::OverallQualityMetric metric(assessor);
  for (const int threads : {1, QThread::idealThreadCount()}) {
    core::ThreadPoolTaskRunner workers(threads);
    optimiser::Optimiser search(*encoder, metric, workers);
    double speech = 0.0;
    double took = 0.0;
    for (const project::Phrase& phrase : loaded->Phrases()) {
      const auto options = project::EffectiveEncoderOptions(
          loaded->Settings().default_encoder, phrase.encoder);
      const auto check = loaded->BudgetCheckFor(phrase.id);
      ASSERT_TRUE(options.IsOk() && check.IsOk()) << phrase.name;
      const std::vector<double> source(phrase.audio.begin(),
                                       phrase.audio.end());
      const auto start = std::chrono::steady_clock::now();
      const auto result = search.Run(
          {.source = source,
           .start = *options,
           .limits = {},
           .budget = [&check](int bytes) { return check->Check(bytes); },
           .plan = {}});
      const std::chrono::duration<double> seconds =
          std::chrono::steady_clock::now() - start;
      ASSERT_TRUE(result.IsOk()) << phrase.name;
      ASSERT_NE(result->Best(), nullptr) << phrase.name;
      const double length = static_cast<double>(phrase.audio.size()) /
                            project::kProjectSampleRate;
      speech += length;
      took += seconds.count();
      if (report.is_open()) {
        report << threads << '\t' << phrase.name << '\t' << length << '\t'
               << result->history.size() << '\t' << seconds.count() << '\t'
               << result->Start()->score << '\t' << result->Best()->score
               << '\n';
      }
    }
    std::cout << "optimise on " << threads << " thread(s): " << took / speech
              << " s per second of speech\n";
  }
}

}  // namespace
}  // namespace speak5220::testing
