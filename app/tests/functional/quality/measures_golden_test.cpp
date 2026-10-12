/*
 * File:        measures_golden_test.cpp
 * Module:      quality tests
 * Purpose:     Compares the quality measures on the corpus subset's
 *              reference encodings with independent implementations'
 *              values (plan Phase 8)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/measures_golden.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <map>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/quality/envelope_distortion.h"
#include "core/quality/estoi.h"
#include "core/quality/lpc_envelope.h"
#include "core/quality/pitch_errors.h"
#include "core/quality/pitch_estimator.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::testing {
namespace {

class MeasuresGoldenTest : public ::testing::Test {
 protected:
  using Encodings = std::vector<PreparedEncoding>;

  // The prepared encodings, made on first use (a few seconds) and shared
  // by the suite's tests; empty, with a failure, if they cannot be made.
  static const Encodings& Prepared() {
    static const Encodings kPrepared = [] {
      core::StatusOr<Encodings> prepared = PrepareReferenceEncodings();
      EXPECT_TRUE(prepared.IsOk()) << prepared.GetStatus().ToString();
      return prepared.IsOk() ? std::move(*prepared) : Encodings{};
    }();
    return kPrepared;
  }

  // The reference scores of `e`, its set read once.
  static const ReferenceMeasures* Reference(const PreparedEncoding& e) {
    static std::map<std::string, std::map<std::string, ReferenceMeasures>> sets;
    if (!sets.contains(e.set_name)) {
      const auto scores = ReadMeasureScores(MeasuresGoldenDir() / "scores" /
                                            (e.set_name + ".tsv"));
      EXPECT_TRUE(scores.IsOk()) << scores.GetStatus().ToString();
      sets[e.set_name] =
          scores.IsOk() ? *scores : decltype(sets)::mapped_type{};
    }
    const auto& set = sets[e.set_name];
    const auto it = set.find(e.name);
    return it == set.end() ? nullptr : &it->second;
  }

  // Plan Phase 8, task 9: SPTK's REAPER and Harvest give the original
  // tools' tracks of the same signals, frame for frame, and so the same
  // errors.
  static void ExpectPitchMatches(core::quality::PitchAlgorithm algorithm);
};

// Plan Phase 8, task 7: the alignment and the voice activity mask match
// the reference's. libfvad is the WebRTC code that py-webrtcvad wraps, and
// the alignment the same as speechqual.py's, so both are exact.
TEST_F(MeasuresGoldenTest, AlignmentAndMaskMatchTheReference) {
  ASSERT_EQ(Prepared().size(), 32U);
  std::map<std::string, std::map<std::string, std::vector<bool>>> masks;
  for (const PreparedEncoding& e : Prepared()) {
    const std::string id = e.set_name + "/" + e.name;
    if (!masks.contains(e.set_name)) {
      const auto read =
          ReadMasks(MeasuresGoldenDir() / "masks" / (e.set_name + ".tsv"));
      ASSERT_TRUE(read.IsOk()) << read.GetStatus().ToString();
      masks[e.set_name] = *read;
    }
    const ReferenceMeasures* ref = Reference(e);
    ASSERT_NE(ref, nullptr) << id;
    EXPECT_EQ(e.lag, ref->lag) << id;
    const auto mask = masks[e.set_name].find(e.name);
    ASSERT_NE(mask, masks[e.set_name].end()) << id;
    EXPECT_EQ(e.prepared.speech, mask->second) << id;
  }
}

// Plan Phase 8, task 8: the mean distortion within 0.01 dB of the NumPy
// reference's. A frame's distortion can fall either side of a threshold
// between the two, so the outlier percentages may differ by one frame.
TEST_F(MeasuresGoldenTest, EnvelopeDistortionMatchesTheReference) {
  core::quality::SptkLpcAnalysis lpc;
  double worst = 0.0;
  for (const PreparedEncoding& e : Prepared()) {
    const std::string id = e.set_name + "/" + e.name;
    const ReferenceMeasures* ref = Reference(e);
    ASSERT_NE(ref, nullptr) << id;
    const auto d = core::quality::MeasureEnvelopeDistortion(e.prepared, lpc);
    ASSERT_TRUE(d.IsOk()) << id << ": " << d.GetStatus().ToString();
    ASSERT_GT(d->frames, 0) << id;
    const double one_frame = 100.0 / d->frames + 1e-9;
    worst = std::max(worst, std::fabs(d->mean_db - ref->lsd_db));
    EXPECT_NEAR(d->mean_db, ref->lsd_db, 0.01) << id;
    EXPECT_NEAR(d->over_2db, ref->lsd_over_2db, one_frame) << id;
    EXPECT_NEAR(d->over_4db, ref->lsd_over_4db, one_frame) << id;
  }
  std::cout << "worst envelope distortion difference: " << worst << " dB\n";
}

// The fraction of frames, over the shorter, where two tracks agree on the
// voicing and, where both are voiced, on F0 within `cents`.
double Agreement(const std::vector<double>& a, const std::vector<double>& b,
                 double cents) {
  const std::size_t n = std::min(a.size(), b.size());
  std::size_t agree = 0;
  for (std::size_t i = 0; i < n; ++i) {
    if ((a[i] > 0.0) != (b[i] > 0.0)) {
      continue;
    }
    if (a[i] > 0.0 && std::fabs(1200.0 * std::log2(b[i] / a[i])) > cents) {
      continue;
    }
    ++agree;
  }
  return n == 0 ? 1.0 : static_cast<double>(agree) / static_cast<double>(n);
}

// REAPER's tool writes F0 as text to six decimals, which moves the RMSE by
// up to a few millionths of a cent.
constexpr double kCentsTolerance = 1e-4;

void MeasuresGoldenTest::ExpectPitchMatches(
    core::quality::PitchAlgorithm algorithm) {
  core::quality::SptkPitchEstimator estimator(algorithm);
  const bool reaper = algorithm == core::quality::PitchAlgorithm::kReaper;
  for (const PreparedEncoding& e : Prepared()) {
    const std::string id = e.set_name + "/" + e.name;
    const auto tracks = ReadTracks(MeasuresGoldenDir() / "tracks" / e.set_name /
                                   (e.name + ".tsv"));
    ASSERT_TRUE(tracks.IsOk()) << id << ": " << tracks.GetStatus().ToString();
    const auto original = estimator.Track(e.prepared.original);
    const auto output = estimator.Track(e.prepared.output);
    ASSERT_TRUE(original.IsOk() && output.IsOk()) << id;
    const std::vector<double>& ref_original =
        reaper ? tracks->reaper_original : tracks->harvest_original;
    const std::vector<double>& ref_output =
        reaper ? tracks->reaper_output : tracks->harvest_output;
    EXPECT_EQ(Agreement(*original, ref_original, 0.01), 1.0) << id;
    EXPECT_EQ(Agreement(*output, ref_output, 0.01), 1.0) << id;

    const ReferenceMeasures* ref = Reference(e);
    if (ref == nullptr) {
      ADD_FAILURE() << id << ": no reference";
      continue;
    }
    // SPTK pads a track to whole frames with its last value, where REAPER's
    // tool may stop a frame earlier: the errors are compared over the
    // reference's frames.
    const core::quality::PitchErrors errors = core::quality::ComparePitch(
        std::span(*original).first(
            std::min(original->size(), ref_original.size())),
        std::span(*output).first(std::min(output->size(), ref_output.size())),
        e.prepared.speech);
    EXPECT_NEAR(errors.gross_error, reaper ? ref->reaper_gpe : ref->harvest_gpe,
                1e-9)
        << id;
    EXPECT_NEAR(errors.f0_rmse_cents,
                reaper ? ref->reaper_f0_rmse_cents : ref->harvest_f0_rmse_cents,
                kCentsTolerance)
        << id;
    EXPECT_NEAR(errors.voicing_error,
                reaper ? ref->reaper_vde : ref->harvest_vde, 1e-9)
        << id;
  }
}

TEST_F(MeasuresGoldenTest, ReaperMatchesItsCommandLineTool) {
  ExpectPitchMatches(core::quality::PitchAlgorithm::kReaper);
}

TEST_F(MeasuresGoldenTest, HarvestMatchesPyworld) {
  ExpectPitchMatches(core::quality::PitchAlgorithm::kHarvest);
}

// Plan Phase 8, task 10: the ESTOI port within 0.005 of pystoi's.
TEST_F(MeasuresGoldenTest, EstoiMatchesPystoi) {
  double worst = 0.0;
  for (const PreparedEncoding& e : Prepared()) {
    const std::string id = e.set_name + "/" + e.name;
    const ReferenceMeasures* ref = Reference(e);
    ASSERT_NE(ref, nullptr) << id;
    const auto estoi = core::quality::MeasureIntelligibility(e.prepared);
    ASSERT_TRUE(estoi.IsOk()) << id << ": " << estoi.GetStatus().ToString();
    worst = std::max(worst, std::fabs(*estoi - ref->estoi));
    EXPECT_NEAR(*estoi, ref->estoi, 0.005) << id;
  }
  std::cout << "worst ESTOI difference: " << worst << "\n";
}

}  // namespace
}  // namespace speak5220::testing
