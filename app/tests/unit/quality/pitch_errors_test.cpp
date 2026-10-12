/*
 * File:        pitch_errors_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the pitch and voicing measures: cents, gross
 *              errors, voicing disagreements and the mask
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pitch_errors.h"

#include <cmath>
#include <vector>

#include "core/quality/signal_preparation.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/quality_mocks.h"

namespace speak5220::core::quality {
namespace {

using ::testing::_;
using ::testing::Return;

// `hz` raised by `cents`.
double Shifted(double hz, double cents) {
  return hz * std::pow(2.0, cents / 1200.0);
}

TEST(PitchErrorsTest, ATwentyFiveCentShiftGivesTwentyFiveCents) {
  const std::vector<double> original = {100.0, 150.0, 200.0, 220.0};
  std::vector<double> output;
  output.reserve(original.size());
  for (const double f : original) {
    output.push_back(Shifted(f, 25.0));
  }
  const PitchErrors e =
      ComparePitch(original, output, {true, true, true, true});
  EXPECT_NEAR(e.f0_rmse_cents, 25.0, 1e-9);
  EXPECT_EQ(e.gross_error, 0.0);
  EXPECT_EQ(e.voicing_error, 0.0);
  EXPECT_EQ(e.frames, 4);
  EXPECT_EQ(e.voiced_frames, 4);
}

TEST(PitchErrorsTest, AnOctaveErrorIsGrossAndLeftOutOfTheRmse) {
  const std::vector<double> original = {100.0, 100.0, 100.0, 100.0};
  const std::vector<double> output = {200.0, Shifted(100.0, 10.0),
                                      Shifted(100.0, -10.0), 100.0};
  const PitchErrors e =
      ComparePitch(original, output, {true, true, true, true});
  EXPECT_EQ(e.gross_error, 25.0);
  // The other three: 10, -10 and 0 cents.
  EXPECT_NEAR(e.f0_rmse_cents, std::sqrt(200.0 / 3.0), 1e-9);
}

TEST(PitchErrorsTest, TwentyPercentIsNotYetGross) {
  const PitchErrors e =
      ComparePitch(std::vector<double>{100.0, 100.0},
                   std::vector<double>{119.0, 79.0}, {true, true});
  // 19 % up is fine; 21 % down is gross.
  EXPECT_EQ(e.gross_error, 50.0);
}

TEST(PitchErrorsTest, VoicingDisagreementsAreCounted) {
  // Frames 0 and 1 disagree; 2 is unvoiced in both, 3 voiced in both.
  const std::vector<double> original = {120.0, 0.0, 0.0, 120.0};
  const std::vector<double> output = {0.0, 130.0, 0.0, 120.0};
  const PitchErrors e =
      ComparePitch(original, output, {true, true, true, true});
  EXPECT_EQ(e.voicing_error, 50.0);
  EXPECT_EQ(e.voiced_frames, 1);
  EXPECT_EQ(e.gross_error, 0.0);
}

TEST(PitchErrorsTest, FramesOutsideTheMaskAreNotCounted) {
  const std::vector<double> original = {120.0, 120.0, 120.0};
  const std::vector<double> output = {0.0, 240.0, 120.0};
  const PitchErrors e = ComparePitch(original, output, {false, false, true});
  EXPECT_EQ(e.frames, 1);
  EXPECT_EQ(e.voicing_error, 0.0);
  EXPECT_EQ(e.gross_error, 0.0);
}

TEST(PitchErrorsTest, TracksOfDifferentLengthsAreComparedWhereBothAre) {
  const PitchErrors e =
      ComparePitch(std::vector<double>{120.0, 120.0, 120.0},
                   std::vector<double>{120.0}, {true, true, true});
  EXPECT_EQ(e.frames, 1);
}

TEST(PitchErrorsTest, NothingVoicedGivesNoPitchErrors) {
  const PitchErrors e =
      ComparePitch(std::vector<double>{0.0, 0.0}, std::vector<double>{0.0, 0.0},
                   {true, true});
  EXPECT_EQ(e.gross_error, 0.0);
  EXPECT_EQ(e.f0_rmse_cents, 0.0);
  EXPECT_EQ(e.voicing_error, 0.0);
}

TEST(PitchErrorsTest, MeasureTracksBothSignals) {
  PreparedSignals p;
  p.original.assign(160, 0.5);
  p.output.assign(160, 0.25);
  p.speech = {true, true};
  testing::MockPitchEstimator estimator;
  EXPECT_CALL(estimator, Track(_))
      .WillOnce(Return(std::vector<double>{100.0, 100.0}))
      .WillOnce(Return(std::vector<double>{100.0, 0.0}));
  const StatusOr<PitchErrors> e = MeasurePitchErrors(p, estimator);
  ASSERT_TRUE(e.IsOk()) << e.GetStatus().ToString();
  EXPECT_EQ(e->voicing_error, 50.0);
}

TEST(PitchErrorsTest, EstimatorFailureIsReturned) {
  PreparedSignals p;
  p.original.assign(160, 0.5);
  p.output = p.original;
  testing::MockPitchEstimator estimator;
  EXPECT_CALL(estimator, Track(_))
      .WillOnce(Return(FailedPreconditionError("too short")));
  EXPECT_EQ(MeasurePitchErrors(p, estimator).GetStatus().Code(),
            StatusCode::kFailedPrecondition);
}

}  // namespace
}  // namespace speak5220::core::quality
