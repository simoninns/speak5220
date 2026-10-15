/*
 * File:        phrom_roundtrip_test.cpp
 * Module:      encoder tests
 * Purpose:     The PHROM round trip: encodes the chip's output of every
 *              word of a PHROM and compares the encoding with the word's
 *              own frames
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <memory>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/project/phrase.h"
#include "core/project/project_serializer.h"
#include "core/quality/frame_track_comparison.h"
#include "core/quality/loudness.h"
#include "core/synth/reference_synthesizer.h"
#include "core/util/status.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

namespace quality = core::quality;
using core::lpc::Frame;

constexpr double kPercent = 100.0;
// A pitch difference beyond this many semitones is a gross error.
constexpr double kGrossSemitones = 6.0;

// The words phrom_roundtrip.py wrote, as index.tsv lists them.
struct Word {
  std::string stem;
  bool speech = false;
  std::vector<Frame> frames;
  std::vector<std::uint8_t> bytes;
};

core::StatusOr<std::vector<Word>> ReadWords(const std::filesystem::path& dir) {
  core::StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(dir / "index.tsv");
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  std::vector<Word> words;
  for (const auto& row : *rows) {
    Word word;
    word.stem = row.at("stem");
    word.speech = row.at("speech") == "1";
    const std::filesystem::path frames_path =
        dir / (word.stem + ".frames.yaml");
    core::StatusOr<std::vector<std::uint8_t>> yaml = ReadBytes(frames_path);
    if (!yaml.IsOk()) {
      return yaml.GetStatus();
    }
    core::StatusOr<core::project::CachedEncoding> read =
        core::project::ReadFramesYaml(*yaml, frames_path.string());
    if (!read.IsOk()) {
      return read.GetStatus();
    }
    word.frames = read->frames;
    core::StatusOr<std::vector<std::uint8_t>> bytes =
        ReadBytes(dir / (word.stem + ".bin"));
    if (!bytes.IsOk()) {
      return bytes.GetStatus();
    }
    word.bytes = *bytes;
    words.push_back(std::move(word));
  }
  return words;
}

// Both tracks' loudness.
struct LoudnessPair {
  quality::Loudness reference;
  quality::Loudness encoded;
};

quality::Loudness LoudnessOf(std::span<const Frame> frames) {
  const core::synth::ReferenceSynthesis spoken =
      core::synth::ReferenceSynthesizer::Synthesize(frames);
  return quality::MeasureLoudness(spoken.codes, frames, spoken.wrapped_samples);
}

double MeanOf(const std::vector<double>& values) {
  return values.empty() ? 0.0 : core::dsp::Mean(values);
}

double MedianOf(const std::vector<double>& values) {
  return values.empty() ? 0.0 : core::dsp::Median(values);
}

double MeanAbs(const std::vector<double>& values) {
  std::vector<double> magnitudes;
  magnitudes.reserve(values.size());
  for (const double v : values) {
    magnitudes.push_back(std::fabs(v));
  }
  return MeanOf(magnitudes);
}

double ShareOver(const std::vector<double>& values, double limit) {
  if (values.empty()) {
    return 0.0;
  }
  int over = 0;
  for (const double v : values) {
    over += std::fabs(v) > limit ? 1 : 0;
  }
  return kPercent * over / static_cast<double>(values.size());
}

int OnTime(const quality::BoundaryLags& lags) {
  return lags.at(static_cast<std::size_t>(-quality::kFirstBoundaryLag));
}

std::string Lags(const quality::BoundaryLags& lags) {
  std::ostringstream text;
  for (int lag = quality::kFirstBoundaryLag; lag <= quality::kLastBoundaryLag;
       ++lag) {
    text << (lag > 0 ? "+" : "") << lag << ": "
         << lags.at(static_cast<std::size_t>(lag - quality::kFirstBoundaryLag))
         << ", ";
  }
  text << "none: " << lags.back();
  return text.str();
}

std::string Runs(const quality::FrameTrackFacts& facts) {
  std::ostringstream text;
  for (std::size_t b = 0; b < facts.repeat_runs.size(); ++b) {
    text << (b > 0 ? " / " : "") << facts.repeat_runs.at(b);
  }
  text << ", longest " << facts.longest_repeat_run;
  return text.str();
}

constexpr std::string_view kReportHeader =
    "name\tframes_ref\tframes_enc\tbits_ref\tbits_enc\tshift\tmatched\t"
    "voicing_agree_pct\tvoiced_as_unvoiced_inside\tvoiced_as_unvoiced_onset\t"
    "voiced_as_unvoiced_offset\tloud_onset_frames\tunvoiced_as_voiced\t"
    "onsets\tonsets_on_time\toffsets\toffsets_on_time\t"
    "energy_diff_voiced\tenergy_diff_unvoiced\tshape_db_voiced\t"
    "shape_db_unvoiced\tpitch_diff_abs_st\tpitch_over_6st_pct\t"
    "repeats_ref\trepeats_enc\tsounding_ref\tsounding_enc\tfade_outs_ref\t"
    "fade_outs_enc\tfade_frames_ref\tfade_frames_enc\trms_ref\trms_enc\t"
    "peak_ref\tpeak_enc\tclipped_pct_ref\tclipped_pct_enc\twraps_ref\t"
    "wraps_enc\tbbc_rms_ref\tbbc_rms_enc\ta_weighted_ref\ta_weighted_enc";

// One report row. For a word, `loudness` is its own; for the total, the
// medians over the words (the clipping, a mean; the wraps, a sum).
void WriteRow(std::ostream& out, const std::string& name,
              const quality::FrameTrackComparison& c,
              const LoudnessPair& loudness) {
  const double agree =
      c.matched > 0 ? kPercent * c.voicing_agrees / c.matched : 0.0;
  out << name << '\t' << c.reference.frames << '\t' << c.encoded.frames << '\t'
      << c.reference.bits << '\t' << c.encoded.bits << '\t' << c.shift << '\t'
      << c.matched << '\t' << agree << '\t' << c.voiced_as_unvoiced_inside
      << '\t' << c.voiced_as_unvoiced_onset << '\t'
      << c.voiced_as_unvoiced_offset << '\t' << c.loud_onset_frames << '\t'
      << c.unvoiced_as_voiced << '\t' << c.reference_onsets << '\t'
      << OnTime(c.onset_lags) << '\t' << c.reference_offsets << '\t'
      << OnTime(c.offset_lags) << '\t' << MeanOf(c.voiced_energy_differences)
      << '\t' << MeanOf(c.unvoiced_energy_differences) << '\t'
      << MedianOf(c.voiced_shape_errors_db) << '\t'
      << MedianOf(c.unvoiced_shape_errors_db) << '\t'
      << MeanAbs(c.pitch_differences) << '\t'
      << ShareOver(c.pitch_differences, kGrossSemitones) << '\t'
      << c.reference.repeats << '\t' << c.encoded.repeats << '\t'
      << c.reference.sounding << '\t' << c.encoded.sounding << '\t'
      << c.reference.fade_outs << '\t' << c.encoded.fade_outs << '\t'
      << c.reference.fade_frames << '\t' << c.encoded.fade_frames << '\t'
      << loudness.reference.sounding_rms << '\t'
      << loudness.encoded.sounding_rms << '\t' << loudness.reference.peak
      << '\t' << loudness.encoded.peak << '\t'
      << kPercent * loudness.reference.clipped << '\t'
      << kPercent * loudness.encoded.clipped << '\t'
      << loudness.reference.wrapped_samples << '\t'
      << loudness.encoded.wrapped_samples << '\t'
      << loudness.reference.high_passed_rms << '\t'
      << loudness.encoded.high_passed_rms << '\t'
      << loudness.reference.a_weighted_rms << '\t'
      << loudness.encoded.a_weighted_rms << '\n';
}

// The loudness over the words, as the total row gives it.
quality::Loudness Pooled(const std::vector<quality::Loudness>& words) {
  std::vector<double> rms;
  std::vector<double> peak;
  std::vector<double> clipped;
  std::vector<double> bbc;
  std::vector<double> a_weighted;
  quality::Loudness out;
  for (const quality::Loudness& l : words) {
    rms.push_back(l.sounding_rms);
    peak.push_back(l.peak);
    clipped.push_back(l.clipped);
    bbc.push_back(l.high_passed_rms);
    a_weighted.push_back(l.a_weighted_rms);
    out.wrapped_samples += l.wrapped_samples;
  }
  out.sounding_rms = MedianOf(rms);
  out.peak = static_cast<int>(MedianOf(peak));
  out.clipped = MeanOf(clipped);
  out.high_passed_rms = MedianOf(bbc);
  out.a_weighted_rms = MedianOf(a_weighted);
  return out;
}

void PrintSummary(const quality::FrameTrackComparison& t, const LoudnessPair& l,
                  int words) {
  std::cout << std::fixed << std::setprecision(2);
  std::cout << "phrom-check: " << words << " speech words\n"
            << "  voicing: " << t.matched << " frames matched, "
            << kPercent * t.voicing_agrees / std::max(1, t.matched)
            << " % agree; reference voiced, encoded unvoiced: "
            << t.voiced_as_unvoiced_inside << " inside / "
            << t.voiced_as_unvoiced_onset << " onset (" << t.loud_onset_frames
            << " at energy " << quality::kLoudOnsetEnergy << "+) / "
            << t.voiced_as_unvoiced_offset << " offset; unvoiced as voiced "
            << t.unvoiced_as_voiced << '\n'
            << "  onsets: " << t.reference_onsets << " (encoded lag "
            << Lags(t.onset_lags) << ")\n"
            << "  offsets: " << t.reference_offsets << " (encoded lag "
            << Lags(t.offset_lags) << ")\n"
            << "  energy index, encoded - reference: voiced mean "
            << MeanOf(t.voiced_energy_differences) << ", unvoiced mean "
            << MeanOf(t.unvoiced_energy_differences) << '\n'
            << "  filter shape error, median dB RMS: voiced "
            << MedianOf(t.voiced_shape_errors_db) << ", unvoiced "
            << MedianOf(t.unvoiced_shape_errors_db) << '\n'
            << "  level, encoded - reference: median "
            << MedianOf(t.level_differences_db) << " dB, p10 "
            << (t.level_differences_db.empty()
                    ? 0.0
                    : core::dsp::Percentile(t.level_differences_db, 10.0))
            << ", p90 "
            << (t.level_differences_db.empty()
                    ? 0.0
                    : core::dsp::Percentile(t.level_differences_db, 90.0))
            << '\n'
            << "  pitch: mean |difference| " << MeanAbs(t.pitch_differences)
            << " semitones, " << ShareOver(t.pitch_differences, kGrossSemitones)
            << " % over 6\n"
            << "  repeats: reference " << t.reference.repeats << " of "
            << t.reference.sounding << " (runs 1/2/3/4+ " << Runs(t.reference)
            << "), encoded " << t.encoded.repeats << " of "
            << t.encoded.sounding << " (runs " << Runs(t.encoded) << ")\n"
            << "  fade-outs: reference " << t.reference.fade_outs << " ("
            << t.reference.fade_frames << " frames), encoded "
            << t.encoded.fade_outs << " (" << t.encoded.fade_frames
            << " frames)\n"
            << "  bits: reference " << t.reference.bits << ", encoded "
            << t.encoded.bits << '\n'
            << "  loudness (median per word), reference / encoded: sounding "
               "RMS "
            << l.reference.sounding_rms << " / " << l.encoded.sounding_rms
            << ", peak " << l.reference.peak << " / " << l.encoded.peak
            << ", clipped (mean) " << kPercent * l.reference.clipped << " % / "
            << kPercent * l.encoded.clipped << " %, BBC Micro "
            << l.reference.high_passed_rms << " / " << l.encoded.high_passed_rms
            << ", A-weighted " << l.reference.a_weighted_rms << " / "
            << l.encoded.a_weighted_rms << ", wraps "
            << l.reference.wrapped_samples << " / " << l.encoded.wrapped_samples
            << '\n';
}

// Run by hand by phrom-check.sh: for every word phrom_roundtrip.py wrote
// into SPEAK5220_PHROM_DIR, checks that its frames pack to the image's
// bytes and parse back (speak5220 datasheet §12.11); then encodes each
// speech word's chip output with the options in SPEAK5220_PHROM_OPTIONS
// (as "repeat_lambda=300,voicing_threshold=0.5"; unset for the defaults),
// compares the encoding with the word's frames and writes one row per word
// and a total to SPEAK5220_PHROM_REPORT.
TEST(PhromRoundTripCheck, DISABLED_WritesTheReport) {
  const char* dir = std::getenv("SPEAK5220_PHROM_DIR");
  const char* report_path = std::getenv("SPEAK5220_PHROM_REPORT");
  ASSERT_NE(dir, nullptr) << "set SPEAK5220_PHROM_DIR (phrom-check.sh)";
  ASSERT_NE(report_path, nullptr)
      << "set SPEAK5220_PHROM_REPORT (phrom-check.sh)";
  const char* options_text = std::getenv("SPEAK5220_PHROM_OPTIONS");
  const core::StatusOr<core::encoder::EncoderOptions> options =
      EncoderOptionsFromText(options_text == nullptr ? "" : options_text);
  ASSERT_TRUE(options.IsOk()) << options.GetStatus().ToString();
  const core::StatusOr<std::vector<Word>> words = ReadWords(dir);
  ASSERT_TRUE(words.IsOk()) << words.GetStatus().ToString();
  ASSERT_FALSE(words->empty());
  std::ofstream report(report_path);
  ASSERT_TRUE(report.is_open()) << "cannot write " << report_path;
  report << kReportHeader << '\n';

  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  quality::FrameTrackComparison total;
  std::vector<quality::Loudness> reference_loudness;
  std::vector<quality::Loudness> encoded_loudness;
  int speech_words = 0;
  for (const Word& word : *words) {
    // Datasheet §12.11: the frames pack to the stored bytes (the unused
    // bits of the last byte cleared) and parse back to themselves.
    const core::StatusOr<std::vector<std::uint8_t>> packed =
        core::lpc::FrameCodec::Pack(word.frames);
    ASSERT_TRUE(packed.IsOk()) << word.stem;
    EXPECT_EQ(*packed, word.bytes) << word.stem;
    EXPECT_EQ(core::lpc::FrameCodec::Parse(word.bytes).frames, word.frames)
        << word.stem;
    if (!word.speech) {
      continue;
    }
    const core::StatusOr<WavSamples> wav =
        ReadWavSamples(std::filesystem::path(dir) / (word.stem + ".wav"));
    ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    const core::StatusOr<core::encoder::EncodeResult> encoding =
        encoder->Encode(wav->samples, wav->sample_rate, *options);
    ASSERT_TRUE(encoding.IsOk())
        << word.stem << ": " << encoding.GetStatus().ToString();
    const quality::FrameTrackComparison c =
        quality::CompareFrameTracks(word.frames, encoding->frames);
    const LoudnessPair loudness = {.reference = LoudnessOf(word.frames),
                                   .encoded = LoudnessOf(encoding->frames)};
    WriteRow(report, word.stem, c, loudness);
    quality::AddComparison(total, c);
    reference_loudness.push_back(loudness.reference);
    encoded_loudness.push_back(loudness.encoded);
    ++speech_words;
  }
  const LoudnessPair pooled = {.reference = Pooled(reference_loudness),
                               .encoded = Pooled(encoded_loudness)};
  WriteRow(report, "total", total, pooled);
  PrintSummary(total, pooled, speech_words);
}

}  // namespace
}  // namespace speak5220::testing
