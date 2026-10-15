/*
 * File:        measures_corpus_test.cpp
 * Module:      quality tests
 * Purpose:     Measures the whole corpus for the calibration of the overall
 *              score (the reference encodings, the application's default
 *              and optimised encodings, and the anchor vocoder), and writes
 *              samples of each rating band for the listening check
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/audio/wav_io.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_filters.h"
#include "core/lpc/frame_statistics.h"
#include "core/optimiser/optimiser.h"
#include "core/quality/anchor_vocoder.h"
#include "core/quality/encoded_audio.h"
#include "core/quality/lpc_envelope.h"
#include "core/quality/pitch_estimator.h"
#include "core/quality/quality_assessor.h"
#include "core/quality/quality_measures.h"
#include "core/quality/quality_score.h"
#include "core/quality/signal_preparation.h"
#include "core/quality/voice_activity.h"
#include "core/synth/synth.h"
#include "core/util/task_runner.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

namespace quality = core::quality;

// One row of the report: what was measured, its size, its overall score
// and the measures.
std::string Row(const std::string& name, const std::string& kind, int bytes,
                const quality::QualityMeasurement& m) {
  std::ostringstream row;
  row.precision(10);
  row << name << '\t' << kind << '\t' << bytes << '\t'
      << quality::ScoreQuality(m).overall << '\t' << m.speech_frames << '\t'
      << m.envelope.mean_db << '\t' << m.envelope.over_2db << '\t'
      << m.envelope.over_4db << '\t' << m.pitch.gross_error << '\t'
      << m.pitch.f0_rmse_cents << '\t' << m.pitch.voicing_error << '\t'
      << m.pitch.voiced_frames << '\t';
  if (m.estoi.has_value()) {
    row << *m.estoi;
  } else {
    row << "nan";
  }
  row << '\n';
  return row.str();
}

// Run by app/scripts/corpus-check.sh, not by CTest: measures every WAV
// file in SPEAK5220_CORPUS_DIR as the reference encodings of every option
// set, as the application encodes it with the default options, as the
// anchor vocoder renders it, and (every SPEAK5220_OPTIMISE_STRIDE-th file)
// as Optimise leaves it from the defaults with the default encoding's size
// as its limit. Writes one row per measurement to
// SPEAK5220_MEASURES_REPORT.
TEST(MeasuresCorpusCheck, DISABLED_WritesTheCalibrationData) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* report_path = std::getenv("SPEAK5220_MEASURES_REPORT");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR (corpus-check.sh)";
  ASSERT_NE(report_path, nullptr)
      << "set SPEAK5220_MEASURES_REPORT (corpus-check.sh)";
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
  const auto sets = ReadOptionSets(EncoderGoldenDir() / "option_sets.tsv");
  ASSERT_TRUE(sets.IsOk()) << sets.GetStatus().ToString();
  std::map<std::string, std::map<std::string, ReferenceEncoding>> encodings;
  for (const auto& [set_name, options] : *sets) {
    const auto frames = ReadReferenceEncodings(EncoderGoldenDir() / "frames" /
                                               (set_name + ".tsv"));
    ASSERT_TRUE(frames.IsOk()) << set_name;
    encodings[set_name] = *frames;
  }

  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << "name\tkind\tbytes\tscore\tspeech_frames\tlsd_db\tlsd_over_2db\t"
            "lsd_over_4db\tgpe\tf0_rmse_cents\tvde\tvoiced_frames\testoi\n";
  std::mutex report_mutex;
  std::vector<std::string> failures;

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::ThreadPoolTaskRunner files;
  int index = 0;
  for (const auto& [input_name, input_path] : inputs) {
    const bool optimise = index++ % stride == 0;
    files.Post([&, name = input_name, path = input_path, optimise] {
      core::synth::Synth synth({}, core::synth::SynthCache::kOff);
      quality::FvadVoiceActivity vad(quality::kSpeechDetectorMode);
      quality::SptkLpcAnalysis lpc;
      quality::SptkPitchEstimator pitch(quality::PitchAlgorithm::kReaper);
      const quality::QualityAnalysers analysers{
          .synth = &synth, .vad = &vad, .lpc = &lpc, .pitch = &pitch};
      std::string rows;
      const auto fail = [&](const std::string& what) {
        const std::scoped_lock lock(report_mutex);
        std::string failure = name;
        failure += ": ";
        failure += what;
        failures.push_back(std::move(failure));
      };
      const auto wav = ReadWavSamples(path);
      if (!wav.IsOk()) {
        fail(wav.GetStatus().ToString());
        return;
      }
      const std::vector<double>& source = wav->samples;
      for (const auto& [set_name, set] : encodings) {
        const auto enc = set.find(name);
        if (enc == set.end()) {
          fail("no reference in " + set_name);
          continue;
        }
        const auto m =
            quality::MeasureQuality(source, enc->second.frames, analysers);
        if (!m.IsOk()) {
          fail(set_name + ": " + m.GetStatus().ToString());
          continue;
        }
        rows += Row(name, set_name,
                    core::lpc::SummariseFrames(enc->second.frames).bytes, *m);
      }
      // The anchor: the vocoder of the source after the encoder's 60 Hz
      // high-pass, lined up with it.
      const std::vector<double> filtered = core::encoder::HighPass60Hz(source);
      const auto prepared = quality::PrepareAlignedSignals(
          source, quality::AnchorVocoder(filtered), vad);
      if (prepared.IsOk()) {
        const auto m = quality::MeasurePrepared(*prepared, lpc, pitch);
        if (m.IsOk()) {
          rows += Row(name, "anchor", 0, *m);
        } else {
          fail("anchor: " + m.GetStatus().ToString());
        }
      } else {
        fail("anchor not prepared");
      }
      // The application's defaults, and Optimise from them.
      const auto defaults = encoder->Encode(source, wav->sample_rate, {});
      if (!defaults.IsOk()) {
        fail("not encoded");
        return;
      }
      const int default_bytes =
          core::lpc::SummariseFrames(defaults->frames).bytes;
      const auto m =
          quality::MeasureQuality(source, defaults->frames, analysers);
      if (m.IsOk()) {
        rows += Row(name, "app-default", default_bytes, *m);
      } else {
        fail("app-default: " + m.GetStatus().ToString());
      }
      if (optimise) {
        core::InlineTaskRunner inline_runner;
        quality::QualityAssessor assessor(synth);
        quality::OverallQualityMetric metric(assessor);
        core::optimiser::Optimiser search(*encoder, metric, inline_runner);
        const auto result = search.Run({.source = source,
                                        .start = {},
                                        .limits = {.max_bytes = default_bytes},
                                        .budget = {},
                                        .plan = {}});
        if (result.IsOk() && result->Best() != nullptr) {
          const auto best =
              quality::MeasureQuality(source, result->best_frames, analysers);
          if (best.IsOk()) {
            rows += Row(name, "optimised", result->Best()->bytes, *best);
          } else {
            fail("optimised: " + best.GetStatus().ToString());
          }
        } else {
          fail("not optimised");
        }
      }
      const std::scoped_lock lock(report_mutex);
      report << rows;
    });
  }
  files.Wait();
  for (const std::string& failure : failures) {
    ADD_FAILURE() << failure;
  }
}

// The samples per rating band of the listening check.
constexpr std::size_t kListeningSamples = 5;

// Run by app/scripts/corpus-check.sh with --listening, not by CTest:
// scores every WAV file in SPEAK5220_CORPUS_DIR as the application
// encodes it with the default options, and writes, for each rating band,
// kListeningSamples files spread over the band's scores to
// SPEAK5220_LISTENING_DIR/BAND/: NN_NAME_1_original.wav and
// NN_NAME_2_chip.wav (the chip's output placed against the original, at
// its level), with index.tsv listing every file's score.
TEST(MeasuresCorpusCheck, DISABLED_WritesListeningSamples) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* listening = std::getenv("SPEAK5220_LISTENING_DIR");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR (corpus-check.sh)";
  ASSERT_NE(listening, nullptr)
      << "set SPEAK5220_LISTENING_DIR (corpus-check.sh)";
  const std::filesystem::path out(listening);
  std::vector<std::filesystem::path> inputs;
  for (const auto& entry : std::filesystem::directory_iterator(corpus)) {
    if (entry.path().extension() == ".wav") {
      inputs.push_back(entry.path());
    }
  }
  std::ranges::sort(inputs);
  ASSERT_FALSE(inputs.empty()) << "no WAV files in " << corpus;

  struct Scored {
    std::filesystem::path path;
    quality::QualityMeasurement measures;
    quality::QualityScore score;
    std::vector<double> source;
    std::vector<double> chip;
  };
  std::vector<Scored> scored(inputs.size());
  std::mutex failures_mutex;
  std::vector<std::string> failures;
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::ThreadPoolTaskRunner files;
  for (std::size_t i = 0; i < inputs.size(); ++i) {
    files.Post([&, i] {
      core::synth::Synth synth({}, core::synth::SynthCache::kOff);
      quality::FvadVoiceActivity vad(quality::kSpeechDetectorMode);
      quality::SptkLpcAnalysis lpc;
      quality::SptkPitchEstimator pitch(quality::PitchAlgorithm::kReaper);
      const auto fail = [&](const std::string& what) {
        const std::scoped_lock lock(failures_mutex);
        std::string failure = inputs[i].stem().string();
        failure += ": ";
        failure += what;
        failures.push_back(std::move(failure));
      };
      const auto wav = ReadWavSamples(inputs[i]);
      if (!wav.IsOk()) {
        fail(wav.GetStatus().ToString());
        return;
      }
      const auto encoding = encoder->Encode(wav->samples, wav->sample_rate, {});
      if (!encoding.IsOk()) {
        fail("not encoded");
        return;
      }
      const auto audio =
          quality::RenderEncodedAudio(synth, wav->samples, encoding->frames);
      if (!audio.IsOk()) {
        fail("not spoken");
        return;
      }
      const auto prepared = quality::PrepareSignals(wav->samples, *audio, vad);
      if (!prepared.IsOk()) {
        fail("not prepared");
        return;
      }
      const auto m = quality::MeasurePrepared(*prepared, lpc, pitch);
      if (!m.IsOk()) {
        fail(m.GetStatus().ToString());
        return;
      }
      Scored& s = scored[i];
      s.path = inputs[i];
      s.measures = *m;
      s.score = quality::ScoreQuality(*m);
      s.source = wav->samples;
      s.chip = quality::PlaceOutput(audio->samples, audio->source_offset,
                                    static_cast<int>(wav->samples.size()));
      for (double& v : s.chip) {
        v *= audio->level_gain;
      }
    });
  }
  files.Wait();
  for (const std::string& failure : failures) {
    ADD_FAILURE() << failure;
  }
  std::erase_if(scored, [](const Scored& s) { return s.path.empty(); });
  std::ranges::sort(scored, {},
                    [](const Scored& s) { return s.score.overall; });

  std::filesystem::create_directories(out);
  std::ofstream index(out / "index.tsv");
  ASSERT_TRUE(index.is_open());
  index << "name\trating\toverall\tenvelope\tpitch\tintelligibility\tlsd_db\t"
           "lsd_over_4db\tgpe\tf0_rmse_cents\tvde\testoi\tsample\n";
  core::audio::SndfileWavWriter writer;
  const auto to_float = [](const std::vector<double>& x) {
    return std::vector<float>(x.begin(), x.end());
  };
  for (const quality::QualityRating rating :
       {quality::QualityRating::kPoor, quality::QualityRating::kFair,
        quality::QualityRating::kGood}) {
    std::vector<const Scored*> band;
    for (const Scored& s : scored) {
      if (s.score.rating == rating) {
        band.push_back(&s);
      }
    }
    // kListeningSamples spread evenly over the band, from its lowest score
    // to its highest.
    std::vector<std::size_t> picks;
    for (std::size_t k = 0; k < kListeningSamples && !band.empty(); ++k) {
      const std::size_t at =
          band.size() == 1 ? 0
                           : k * (band.size() - 1) / (kListeningSamples - 1);
      if (picks.empty() || picks.back() != at) {
        picks.push_back(at);
      }
    }
    const std::string label(quality::RatingLabel(rating));
    std::filesystem::create_directories(out / label);
    for (std::size_t b = 0; b < band.size(); ++b) {
      const Scored& s = *band[b];
      const auto pick = std::ranges::find(picks, b);
      std::string sample = "-";
      if (pick != picks.end()) {
        std::ostringstream stem;
        stem << (pick - picks.begin() + 1) << '_' << s.path.stem().string();
        sample = label + "/" + stem.str();
        for (const auto& [suffix, signal] :
             {std::pair("_1_original.wav", &s.source),
              std::pair("_2_chip.wav", &s.chip)}) {
          const core::Status written = writer.Write(
              out / label / (stem.str() + suffix), to_float(*signal), 8000,
              core::audio::WavOutputFormat::kPcm16);
          EXPECT_TRUE(written.IsOk()) << written.ToString();
        }
      }
      const auto& c = s.score.components;
      index << s.path.stem().string() << '\t' << label << '\t'
            << s.score.overall << '\t' << c.envelope << '\t' << c.pitch << '\t'
            << (c.intelligibility.has_value() ? *c.intelligibility
                                              : std::nan(""))
            << '\t' << s.measures.envelope.mean_db << '\t'
            << s.measures.envelope.over_4db << '\t'
            << s.measures.pitch.gross_error << '\t'
            << s.measures.pitch.f0_rmse_cents << '\t'
            << s.measures.pitch.voicing_error << '\t'
            << (s.measures.estoi.has_value() ? *s.measures.estoi : std::nan(""))
            << '\t' << sample << '\n';
    }
  }
}

}  // namespace
}  // namespace speak5220::testing
