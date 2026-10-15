/*
 * File:        phoneme_check_test.cpp
 * Module:      encoder tests
 * Purpose:     The phoneme check: encodes the phoneme test set and reports
 *              how each word's S, SH, CH, K, T or F and its vowel come out
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
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_analyser.h"
#include "core/encoder/level_setter.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_statistics.h"
#include "core/preprocess/preprocess.h"
#include "core/quality/frame_track_comparison.h"
#include "core/quality/output_alignment.h"
#include "core/quality/pitch_estimator.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/reference_synthesizer.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

namespace quality = core::quality;
using core::lpc::Frame;
using core::lpc::FrameType;

constexpr double kPercent = 100.0;
// A word whose gross pitch error is above this many percent is listed in
// the summary.
constexpr double kPitchLockPercent = 20.0;

// What the encoding did with one word's consonant and vowel.
struct PhonemeRow {
  std::string file;
  std::string consonant;
  std::string position;
  int frames = 0;
  int bytes = 0;
  double score = 0.0;
  double gpe = 0.0;
  double vde = 0.0;
  // The frames of the consonant (by its position, before the first or
  // after the last frame the source is voiced at), and of those, the
  // frames the encoding voices.
  int consonant_frames = 0;
  int consonant_voiced = 0;
  // The median K1 index of the consonant's unvoiced full frames.
  std::optional<double> consonant_k1;
  // The consonant's level against the vowel's in dB, in the source and in
  // the encoding.
  std::optional<double> consonant_db_source;
  std::optional<double> consonant_db_encoded;
  // Unvoiced frames at quality::kLoudOnsetEnergy or above just before a
  // voiced frame.
  int loud_onsets = 0;
  int fade_frames = 0;
  // Repeats among the voiced frames, in percent.
  double voiced_repeat_pct = 0.0;
};

bool Voiced(const Frame& frame) {
  const FrameType type = core::lpc::FrameKind(frame);
  return (type == FrameType::kVoiced || type == FrameType::kRepeat) &&
         frame.pitch != 0;
}

bool Unvoiced(const Frame& frame) {
  const FrameType type = core::lpc::FrameKind(frame);
  return (type == FrameType::kUnvoiced || type == FrameType::kRepeat) &&
         frame.pitch == 0;
}

// The power ratio in dB of the frames `a` to the frames `b` of `levels`.
std::optional<double> RatioDb(const std::vector<double>& levels,
                              const std::vector<int>& a,
                              const std::vector<int>& b) {
  const auto power = [&](const std::vector<int>& frames) {
    double sum = 0.0;
    for (const int j : frames) {
      const double level = levels.at(static_cast<std::size_t>(j));
      sum += level * level;
    }
    return frames.empty() ? 0.0 : sum / static_cast<double>(frames.size());
  };
  const double pa = power(a);
  const double pb = power(b);
  if (pa <= 0.0 || pb <= 0.0) {
    return std::nullopt;
  }
  return 10.0 * std::log10(pa / pb);
}

std::ostream& operator<<(std::ostream& out, const std::optional<double>& v) {
  if (v.has_value()) {
    out << *v;
  }
  return out;
}

// Analyses one word. `source` is the preprocessed recording the encoding
// was made from.
PhonemeRow Analyse(const std::map<std::string, std::string>& index_row,
                   std::span<const double> source,
                   const core::encoder::EncodeResult& encoding,
                   const quality::QualityAssessment& assessment,
                   std::span<const double> source_f0) {
  PhonemeRow row;
  row.file = index_row.at("file");
  row.consonant = index_row.at("consonant");
  row.position = index_row.at("position");
  const std::vector<Frame>& frames = encoding.frames;
  row.frames = static_cast<int>(frames.size());
  row.bytes = core::lpc::SummariseFrames(frames).bytes;
  row.score = assessment.score.overall;
  row.gpe = assessment.measures.pitch.gross_error;
  row.vde = assessment.measures.pitch.voicing_error;
  row.fade_frames = quality::DescribeFrameTrack(frames).fade_frames;

  int voiced = 0;
  int voiced_repeats = 0;
  for (std::size_t j = 0; j < frames.size(); ++j) {
    if (Voiced(frames[j])) {
      ++voiced;
      voiced_repeats += frames[j].repeat ? 1 : 0;
      if (j > 0 && Unvoiced(frames[j - 1]) &&
          frames[j - 1].energy >= quality::kLoudOnsetEnergy) {
        ++row.loud_onsets;
      }
    }
  }
  row.voiced_repeat_pct = voiced > 0 ? kPercent * voiced_repeats / voiced : 0.0;
  if (!encoding.details.has_value()) {
    return row;
  }

  // Frame j's values are reached at output sample kReachedSample + 200 j,
  // which lines up with input sample centres[j].
  const core::encoder::EncodeDetails& details = *encoding.details;
  const std::size_t planned = std::min(details.centres.size(), frames.size());
  std::vector<double> source_level(planned);
  std::vector<bool> source_voiced(planned);
  double loudest = 0.0;
  for (std::size_t j = 0; j < planned; ++j) {
    source_level[j] = core::encoder::FrameRms(source, details.centres[j]);
    loudest = std::max(loudest, source_level[j]);
    const auto hop =
        static_cast<std::size_t>(details.centres[j] / quality::kPitchHop);
    source_voiced[j] = hop < source_f0.size() && source_f0[hop] > 0.0;
  }
  const double floor = loudest * std::pow(10.0, -quality::kSpeechDb / 20.0);
  std::optional<std::size_t> first_voiced;
  std::optional<std::size_t> last_voiced;
  std::vector<int> vowel;
  for (std::size_t j = 0; j < planned; ++j) {
    if (source_voiced[j]) {
      first_voiced = first_voiced.value_or(j);
      last_voiced = j;
      vowel.push_back(static_cast<int>(j));
    }
  }
  if (!first_voiced.has_value() || row.position == "sentence") {
    return row;
  }
  std::vector<int> consonant;
  for (std::size_t j = 0; j < planned; ++j) {
    const bool before = row.position == "initial" && j < *first_voiced;
    const bool after = row.position == "final" && j > *last_voiced;
    if ((before || after) && source_level[j] > floor) {
      consonant.push_back(static_cast<int>(j));
    }
  }
  row.consonant_frames = static_cast<int>(consonant.size());
  std::vector<double> k1;
  for (const int j : consonant) {
    const Frame& frame = frames.at(static_cast<std::size_t>(j));
    row.consonant_voiced += Voiced(frame) ? 1 : 0;
    if (core::lpc::FrameKind(frame) == FrameType::kUnvoiced) {
      k1.push_back(frame.k[0]);
    }
  }
  if (!k1.empty()) {
    row.consonant_k1 = core::dsp::Median(k1);
  }
  const core::synth::ReferenceSynthesis spoken =
      core::synth::ReferenceSynthesizer::Synthesize(frames);
  const std::vector<double> encoded_level =
      core::encoder::MeasureOutputLevels(spoken.codes,
                                         static_cast<int>(planned))
          .level;
  row.consonant_db_source = RatioDb(source_level, consonant, vowel);
  row.consonant_db_encoded = RatioDb(encoded_level, consonant, vowel);
  return row;
}

constexpr std::string_view kReportHeader =
    "file\tconsonant\tposition\tframes\tbytes\tscore\tgpe\tvde\t"
    "consonant_frames\tconsonant_voiced\tconsonant_k1\tconsonant_db_source\t"
    "consonant_db_encoded\tloud_onsets\tfade_frames\tvoiced_repeat_pct";

void WriteRow(std::ostream& out, const PhonemeRow& r) {
  out << r.file << '\t' << r.consonant << '\t' << r.position << '\t' << r.frames
      << '\t' << r.bytes << '\t' << r.score << '\t' << r.gpe << '\t' << r.vde
      << '\t' << r.consonant_frames << '\t' << r.consonant_voiced << '\t'
      << r.consonant_k1 << '\t' << r.consonant_db_source << '\t'
      << r.consonant_db_encoded << '\t' << r.loud_onsets << '\t'
      << r.fade_frames << '\t' << r.voiced_repeat_pct << '\n';
}

void PrintSummary(const std::vector<PhonemeRow>& rows) {
  // Per consonant and position: files, consonant frames voiced, the mean
  // change of the consonant's level against the vowel, fade frames.
  struct Group {
    int files = 0;
    int frames = 0;
    int voiced = 0;
    std::vector<double> change;
    int fades = 0;
  };
  std::map<std::string, Group> groups;
  int loud_onsets = 0;
  std::cout << std::fixed << std::setprecision(1);
  std::cout << "phoneme-check: " << rows.size() << " files\n";
  for (const PhonemeRow& r : rows) {
    loud_onsets += r.loud_onsets;
    if (r.gpe > kPitchLockPercent) {
      std::cout << "  pitch: " << r.file << " GPE " << r.gpe << " %\n";
    }
    if (r.position == "sentence" || r.consonant.empty()) {
      continue;
    }
    Group& g = groups[r.consonant + " " + r.position];
    ++g.files;
    g.frames += r.consonant_frames;
    g.voiced += r.consonant_voiced;
    g.fades += r.fade_frames;
    if (r.consonant_db_source && r.consonant_db_encoded) {
      g.change.push_back(*r.consonant_db_encoded - *r.consonant_db_source);
    }
  }
  for (const auto& [name, g] : groups) {
    std::cout << "  " << name << ": " << g.files << " files, " << g.voiced
              << " of " << g.frames
              << " consonant frames voiced, level against the vowel "
              << (g.change.empty() ? 0.0 : core::dsp::Mean(g.change))
              << " dB from the source's, " << g.fades << " fade frames\n";
  }
  std::cout << "  unvoiced frames at energy " << quality::kLoudOnsetEnergy
            << "+ before a voiced frame: " << loud_onsets << '\n';
}

// Run by hand by phoneme-check.sh: encodes every file phoneme_corpus.py
// listed in SPEAK5220_PHONEME_DIR/index.tsv with the options in
// SPEAK5220_PHONEME_OPTIONS (as "repeat_lambda=300"; unset for the
// defaults) and writes, per file, to SPEAK5220_PHONEME_REPORT: its score,
// gross pitch and voicing errors, how its consonant's frames were coded
// (the frames before the first or after the last at which REAPER finds the
// source voiced, that are within 35 dB of its loudest), loud unvoiced
// frames before voicing, fade frames and the share of repeats in voiced
// sound.
TEST(PhonemeCheck, DISABLED_WritesTheReport) {
  const char* dir = std::getenv("SPEAK5220_PHONEME_DIR");
  const char* report_path = std::getenv("SPEAK5220_PHONEME_REPORT");
  ASSERT_NE(dir, nullptr) << "set SPEAK5220_PHONEME_DIR (phoneme-check.sh)";
  ASSERT_NE(report_path, nullptr)
      << "set SPEAK5220_PHONEME_REPORT (phoneme-check.sh)";
  const char* options_text = std::getenv("SPEAK5220_PHONEME_OPTIONS");
  const core::StatusOr<core::encoder::EncoderOptions> options =
      EncoderOptionsFromText(options_text == nullptr ? "" : options_text);
  ASSERT_TRUE(options.IsOk()) << options.GetStatus().ToString();
  const auto index = ReadTable(std::filesystem::path(dir) / "index.tsv");
  ASSERT_TRUE(index.IsOk()) << index.GetStatus().ToString();
  ASSERT_FALSE(index->empty());
  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << kReportHeader << '\n';

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  quality::QualityAssessor assessor(synth);
  quality::SptkPitchEstimator reaper(quality::PitchAlgorithm::kReaper);
  std::vector<PhonemeRow> rows;
  for (const auto& entry : *index) {
    const std::string& file = entry.at("file");
    const auto wav = ReadWavSamples(std::filesystem::path(dir) / file);
    ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    const auto encoding =
        encoder->Encode(wav->samples, wav->sample_rate, *options);
    ASSERT_TRUE(encoding.IsOk()) << file;
    const std::vector<double> source =
        core::preprocess::Preprocess(wav->samples, options->preprocess).samples;
    const auto assessed = assessor.Assess(source, encoding->frames);
    ASSERT_TRUE(assessed.IsOk())
        << file << ": " << assessed.GetStatus().ToString();
    const auto f0 = reaper.Track(source);
    ASSERT_TRUE(f0.IsOk()) << file << ": " << f0.GetStatus().ToString();
    rows.push_back(Analyse(entry, source, *encoding, *assessed, *f0));
    WriteRow(report, rows.back());
  }
  PrintSummary(rows);
}

}  // namespace
}  // namespace speak5220::testing
