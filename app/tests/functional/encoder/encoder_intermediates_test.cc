/*
 * File:        encoder_intermediates_test.cc
 * Module:      encoder tests
 * Purpose:     Checks each encoder stage against the Python reference's
 *              intermediate values, stage by stage, on the corpus subset
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QJsonArray>
#include <QJsonObject>
#include <QJsonValue>
#include <QString>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/dsp/fft.h"
#include "core/dsp/lpc_analysis.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_filters.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_analyser.h"
#include "core/encoder/level_setter.h"
#include "core/encoder/pitch_tracker.h"
#include "core/encoder/quantiser.h"
#include "core/encoder/repeat_search.h"
#include "core/encoder/voicing_classifier.h"
#include "core/lpc/frame_codec.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

using core::encoder::EncoderOptions;
using core::encoder::FrameFeatures;
using core::encoder::PlanEntry;
using core::encoder::PlanKind;

// The C++ follows the Python function by function and reproduces its
// frames exactly (plan §3.2); its floating-point values agree to within
// rounding. Values are compared to this relative tolerance, with an
// absolute floor for values near zero.
constexpr double kRelative = 1e-9;
constexpr double kAbsolute = 1e-12;

std::vector<std::string> SubsetNames() {
  std::vector<std::string> names;
  const std::filesystem::path dir = EncoderGoldenDir() / "intermediates";
  if (std::filesystem::is_directory(dir)) {
    for (const auto& entry : std::filesystem::directory_iterator(dir)) {
      if (entry.path().extension() == ".json") {
        names.push_back(entry.path().stem().string());
      }
    }
  }
  std::ranges::sort(names);
  return names;
}

void ExpectClose(double expected, double actual, const std::string& what) {
  const double tolerance = std::max(kAbsolute, kRelative * std::abs(expected));
  EXPECT_NEAR(actual, expected, tolerance) << what;
}

void ExpectAllClose(const std::vector<double>& expected,
                    const std::vector<double>& actual,
                    const std::string& what) {
  ASSERT_EQ(actual.size(), expected.size()) << what;
  for (std::size_t i = 0; i < expected.size(); ++i) {
    ExpectClose(expected[i], actual[i], what + "[" + std::to_string(i) + "]");
  }
}

PlanKind KindOf(const QString& name) {
  if (name == "voiced") {
    return PlanKind::kVoiced;
  }
  if (name == "unvoiced") {
    return PlanKind::kUnvoiced;
  }
  if (name == "repeat") {
    return PlanKind::kRepeat;
  }
  return PlanKind::kSilence;
}

std::vector<PlanEntry> PlanOf(const QJsonArray& array) {
  std::vector<PlanEntry> plan;
  for (const auto& v : array) {
    const QJsonObject o = v.toObject();
    plan.push_back({.kind = KindOf(o["kind"].toString()),
                    .k = JsonInts(o["k"].toArray()),
                    .pitch = o["pitch"].toInt()});
  }
  return plan;
}

template <std::size_t N>
std::array<double, N> ArrayOf(const QJsonArray& array) {
  std::array<double, N> out = {};
  for (qsizetype i = 0; i < array.size() && std::cmp_less(i, N); ++i) {
    out.at(static_cast<std::size_t>(i)) = array[i].toDouble();
  }
  return out;
}

class EncoderIntermediatesTest : public ::testing::TestWithParam<std::string> {
 protected:
  void SetUp() override {
    const auto json = ReadJsonObject(EncoderGoldenDir() / "intermediates" /
                                     (GetParam() + ".json"));
    ASSERT_TRUE(json.IsOk()) << json.GetStatus().ToString();
    ref_ = *json;
    const auto wav =
        ReadWavSamples(EncoderGoldenDir() / "inputs" / (GetParam() + ".wav"));
    ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
    input_ = wav->samples;
  }

  // Every step-th value of a signal.
  static std::vector<double> Decimated(const std::vector<double>& x, int step) {
    std::vector<double> out;
    for (std::size_t i = 0; i < x.size(); i += static_cast<std::size_t>(step)) {
      out.push_back(x[i]);
    }
    return out;
  }

  // The reference's frame features, as the analysis gave them.
  std::vector<FrameFeatures> ReferenceFeatures() const {
    std::vector<FrameFeatures> feats;
    for (const auto& v : ref_["analysis"].toObject()["frames"].toArray()) {
      const QJsonObject o = v.toObject();
      feats.push_back({.centre = o["centre"].toInt(),
                       .level = o["level"].toDouble(),
                       .voiced = o["voiced"].toBool(),
                       .period = o["period"].toDouble(),
                       .rv = ArrayOf<core::encoder::kAutocorrelationSize>(
                           o["rv"].toArray()),
                       .ru = ArrayOf<core::encoder::kAutocorrelationSize>(
                           o["ru"].toArray()),
                       .gated = o["gated"].toBool()});
    }
    return feats;
  }

  QJsonObject ref_;
  std::vector<double> input_;
  core::dsp::PocketFft fft_;
  EncoderOptions options_;
};

// tms5220enc.py _sosfiltfilt() with the committed second-order sections.
TEST_P(EncoderIntermediatesTest, FiltersMatchSciPy) {
  const std::vector<double> x = core::encoder::HighPass60Hz(input_);
  const QJsonObject hp = ref_["highpass"].toObject();
  ExpectAllClose(JsonDoubles(hp["values"].toArray()),
                 Decimated(x, hp["step"].toInt()), "high-pass");
  const QJsonObject lp = ref_["lowpass"].toObject();
  ExpectAllClose(JsonDoubles(lp["values"].toArray()),
                 Decimated(core::encoder::LowPass1kHz(x), lp["step"].toInt()),
                 "low-pass");
}

// tms5220enc.py analyse(): the gate, the onset and offset, and the frames.
TEST_P(EncoderIntermediatesTest, SegmentationMatchesReference) {
  core::encoder::FrameAnalyser analyser;
  const auto seg = analyser.Segment(input_, options_);
  ASSERT_TRUE(seg.has_value());
  const QJsonObject an = ref_["analysis"].toObject();
  ExpectClose(an["gate"].toDouble(), seg->gate, "gate");
  ExpectClose(an["loud"].toDouble(), seg->loud, "loud");
  EXPECT_EQ(seg->onset, an["onset"].toInt());
  EXPECT_EQ(seg->offset, an["offset"].toInt());
  EXPECT_EQ(seg->c0, an["c0"].toInt());
  std::vector<int> centres;
  for (const auto& v : an["frames"].toArray()) {
    centres.push_back(v.toObject()["centre"].toInt());
  }
  EXPECT_EQ(seg->centres, centres);
}

// tms5220enc.py _yin(), _candidates(), _tiltAndCrossings() and
// trackPitch().
TEST_P(EncoderIntermediatesTest, PitchTrackMatchesReference) {
  const std::vector<double> x = core::encoder::HighPass60Hz(input_);
  const QJsonObject pitch = ref_["pitch"].toObject();
  const core::encoder::YinAnalysis yin =
      core::encoder::Yin(fft_, core::encoder::LowPass1kHz(x));
  EXPECT_EQ(yin.centres, JsonInts(pitch["hops"].toArray()));
  const QJsonArray hops = pitch["cmndf_hops"].toArray();
  for (qsizetype i = 0; i < hops.size(); ++i) {
    const auto h = static_cast<std::size_t>(hops[i].toInt());
    ASSERT_LT(h, yin.cmndf.size());
    ExpectAllClose(JsonDoubles(pitch["cmndf"].toArray()[i].toArray()),
                   yin.cmndf[h], "cmndf hop " + std::to_string(h));
    const auto cands = core::encoder::PitchCandidates(yin.cmndf[h]);
    const QJsonArray ref_cands = pitch["candidates"].toArray()[i].toArray();
    ASSERT_EQ(cands.size(), static_cast<std::size_t>(ref_cands.size()));
    for (std::size_t c = 0; c < cands.size(); ++c) {
      const QJsonArray pair = ref_cands[static_cast<qsizetype>(c)].toArray();
      ExpectClose(pair[0].toDouble(), cands[c].lag, "candidate lag");
      ExpectClose(pair[1].toDouble(), cands[c].value, "candidate value");
    }
  }
  const core::encoder::VoicingEvidence evidence =
      core::encoder::MeasureVoicingEvidence(x, yin.centres);
  EXPECT_EQ(evidence.tilt, JsonDoubles(pitch["tilt"].toArray()));
  EXPECT_EQ(evidence.crossings, JsonDoubles(pitch["crossings"].toArray()));

  core::encoder::YinPitchTracker tracker(fft_);
  const core::encoder::PitchTrack track = tracker.Track(x, options_);
  ExpectAllClose(JsonDoubles(pitch["periods"].toArray()), track.periods,
                 "periods");
}

// The voicing parts of tms5220enc.py analyse(), from the reference's own
// pitch track.
TEST_P(EncoderIntermediatesTest, VoicingMatchesReference) {
  const QJsonObject pitch = ref_["pitch"].toObject();
  const core::encoder::PitchTrack track{
      .hops = JsonInts(pitch["hops"].toArray()),
      .periods = JsonDoubles(pitch["periods"].toArray())};
  const std::vector<FrameFeatures> ref = ReferenceFeatures();
  std::vector<int> centres;
  centres.reserve(ref.size());
  for (const FrameFeatures& f : ref) {
    centres.push_back(f.centre);
  }
  core::encoder::VoicingClassifier classifier;
  const auto voicing = classifier.Classify(track, centres);
  ASSERT_EQ(voicing.size(), ref.size());
  for (std::size_t j = 0; j < ref.size(); ++j) {
    EXPECT_EQ(voicing[j].voiced, ref[j].voiced) << "frame " << j;
    ExpectClose(ref[j].period, voicing[j].period,
                "period " + std::to_string(j));
  }
}

// tms5220enc.py analyse(): each frame's level and spectra, from the
// reference's voicing.
TEST_P(EncoderIntermediatesTest, FeaturesMatchReference) {
  core::encoder::FrameAnalyser analyser;
  const auto seg = analyser.Segment(input_, options_);
  ASSERT_TRUE(seg.has_value());
  const std::vector<FrameFeatures> ref = ReferenceFeatures();
  std::vector<core::encoder::FrameVoicing> voicing;
  voicing.reserve(ref.size());
  for (const FrameFeatures& f : ref) {
    voicing.push_back({.voiced = f.voiced, .period = f.period});
  }
  const auto feats = analyser.Features(*seg, voicing, options_);
  ASSERT_EQ(feats.size(), ref.size());
  for (std::size_t j = 0; j < ref.size(); ++j) {
    const std::string frame = "frame " + std::to_string(j);
    ExpectClose(ref[j].level, feats[j].level, frame + " level");
    EXPECT_EQ(feats[j].gated, ref[j].gated) << frame;
    for (std::size_t i = 0; i < ref[j].rv.size(); ++i) {
      ExpectClose(ref[j].rv.at(i), feats[j].rv.at(i), frame + " rv");
      ExpectClose(ref[j].ru.at(i), feats[j].ru.at(i), frame + " ru");
    }
  }
}

// tms5220enc.py levinson() and quantiseK() on each frame's own spectrum.
TEST_P(EncoderIntermediatesTest, QuantisationMatchesReference) {
  const QJsonArray frames = ref_["analysis"].toObject()["frames"].toArray();
  const std::vector<FrameFeatures> feats = ReferenceFeatures();
  for (std::size_t j = 0; j < feats.size(); ++j) {
    const QJsonValue own = frames[static_cast<qsizetype>(j)].toObject()["own"];
    if (own.isNull()) {
      continue;
    }
    const std::string frame = "frame " + std::to_string(j);
    const auto& r = feats[j].voiced ? feats[j].rv : feats[j].ru;
    const int order = feats[j].voiced ? core::encoder::kLpcOrder
                                      : core::encoder::kUnvoicedLpcOrder;
    const std::vector<double> k = core::dsp::Levinson(r, order);
    ExpectAllClose(JsonDoubles(own["reflection"].toArray()), k,
                   frame + " reflection");
    const core::encoder::QuantisedK q = core::encoder::QuantiseK(k, r);
    EXPECT_EQ(q.indices, JsonInts(own["k"].toArray())) << frame;
    ExpectClose(own["error"].toDouble(), q.error, frame + " error");
  }
}

// tms5220enc.py _kindsAndK(), from the reference's features.
TEST_P(EncoderIntermediatesTest, PlanMatchesReference) {
  core::encoder::RepeatSearch search;
  EXPECT_EQ(search.Plan(ReferenceFeatures(), options_),
            PlanOf(ref_["plan"].toArray()));
}

// The faint-opening trim, _gainOf() and _setLevels(), from the
// reference's plan, levels and gain.
TEST_P(EncoderIntermediatesTest, LevelsMatchReference) {
  const QJsonObject enc = ref_["encode"].toObject();
  std::vector<PlanEntry> plan = PlanOf(ref_["plan"].toArray());
  std::vector<double> levels;
  for (const FrameFeatures& f : ReferenceFeatures()) {
    levels.push_back(f.level);
  }
  levels.resize(plan.size());
  const core::encoder::OpeningTrim trim =
      core::encoder::TrimFaintOpening(plan, levels);
  EXPECT_EQ(trim.lead, enc["lead"].toInt());
  EXPECT_EQ(trim.dropped, enc["dropped"].toInt());

  const std::vector<PlanEntry> final_plan = PlanOf(enc["plan"].toArray());
  const std::vector<double> final_levels = JsonDoubles(enc["levels"].toArray());
  const auto eff = core::encoder::EffectiveK(final_plan);
  const std::vector<double> ref_gains = JsonDoubles(enc["gains"].toArray());
  ASSERT_EQ(eff.size(), ref_gains.size());
  for (std::size_t j = 0; j < final_plan.size(); ++j) {
    if (final_plan[j].kind == PlanKind::kSilence) {
      continue;
    }
    const int pitch = final_plan[j].pitch;
    const int period =
        pitch != 0
            ? core::lpc::kPitchTable.at(static_cast<std::size_t>(pitch)) + 1
            : 0;
    ExpectClose(ref_gains[j], core::encoder::GainOf(fft_, eff[j], period),
                "gain of frame " + std::to_string(j));
  }

  core::encoder::LevelSetter setter(fft_);
  core::encoder::LevelStats stats;
  EXPECT_EQ(setter.SetLevels(final_plan, final_levels, enc["gain"].toDouble(),
                             options_, &stats),
            JsonInts(enc["energies"].toArray()));
  EXPECT_EQ(stats.silenced, enc["silenced"].toInt());
  EXPECT_EQ(stats.limited, enc["limited"].toInt());
  EXPECT_EQ(stats.sounding, enc["sounding"].toInt());
}

// tms5220enc.py encodeDetailed() end to end.
TEST_P(EncoderIntermediatesTest, EncodingMatchesReference) {
  const auto encoder = core::encoder::MakeEncoder();
  const auto result =
      encoder->Encode(input_, ref_["sample_rate"].toInt(), options_);
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  ASSERT_TRUE(result->details.has_value());
  const QJsonObject enc = ref_["encode"].toObject();
  const auto packed = core::lpc::FrameCodec::Pack(result->frames);
  ASSERT_TRUE(packed.IsOk());
  std::string hex;
  for (const std::uint8_t b : *packed) {
    constexpr std::string_view kDigits = "0123456789abcdef";
    hex += kDigits[b >> 4];
    hex += kDigits[b & 0xF];
  }
  EXPECT_EQ(hex, enc["frames_hex"].toString().toStdString());
  EXPECT_EQ(static_cast<int>(result->frames.size()),
            enc["frame_count"].toInt());
  ExpectClose(enc["gain"].toDouble(), result->details->gain, "gain");
  EXPECT_EQ(result->details->plan, PlanOf(enc["plan"].toArray()));
  EXPECT_EQ(result->details->energies, JsonInts(enc["energies"].toArray()));
  EXPECT_EQ(result->details->lead, enc["lead"].toInt());
  EXPECT_EQ(result->details->c0, ref_["analysis"]
                                     .toObject()["frames"]
                                     .toArray()[enc["lead"].toInt()]
                                     .toObject()["centre"]
                                     .toInt());
}

// A test name for a corpus file: its name without underscores, which
// Google Test does not allow.
std::string CaseName(const ::testing::TestParamInfo<std::string>& param) {
  std::string name;
  for (const char c : param.param) {
    if (c != '_') {
      name += c;
    }
  }
  return name;
}

INSTANTIATE_TEST_SUITE_P(CorpusSubset, EncoderIntermediatesTest,
                         ::testing::ValuesIn(SubsetNames()), CaseName);

// A missing reference directory must fail, not silently run no cases
// (TESTING.md).
TEST(EncoderIntermediatesDataTest, SubsetIsPresent) {
  EXPECT_FALSE(SubsetNames().empty())
      << "no intermediates in " << EncoderGoldenDir() / "intermediates";
}

}  // namespace
}  // namespace speak5220::testing
