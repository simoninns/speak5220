/*
 * File:        quality_score_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the overall quality score: the mappings, the
 *              components, the weighted geometric mean and the rating
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/quality_score.h"

#include <cmath>
#include <optional>

#include "core/quality/quality_measures.h"
#include "gtest/gtest.h"

namespace speak5220::core::quality {
namespace {

using C = ScoreCalibration;

// The midpoint of a range.
double Middle(ScoreRange r) { return (r.poor + r.good) / 2.0; }

// A measurement with every figure at the given point of its range: 0 at
// poor, 1 at good.
QualityMeasurement At(double t, std::optional<double> estoi_t = 0.0) {
  const auto at = [t](ScoreRange r) { return r.poor + t * (r.good - r.poor); };
  QualityMeasurement m;
  m.envelope.mean_db = at(C::kLsd);
  m.envelope.over_4db = at(C::kLargeOutliers);
  m.pitch.gross_error = at(C::kGrossPitch);
  m.pitch.f0_rmse_cents = at(C::kF0Rmse);
  m.pitch.voicing_error = at(C::kVoicing);
  if (estoi_t.has_value()) {
    m.estoi = C::kEstoi.poor + *estoi_t * (C::kEstoi.good - C::kEstoi.poor);
  }
  return m;
}

TEST(QualityScoreTest, MappingIsLinearBetweenPoorAndGoodAndClamped) {
  // Less is better.
  EXPECT_EQ(MapToScore(C::kLsd.poor, C::kLsd), 0.0);
  EXPECT_EQ(MapToScore(C::kLsd.good, C::kLsd), 1.0);
  EXPECT_NEAR(MapToScore(Middle(C::kLsd), C::kLsd), 0.5, 1e-12);
  EXPECT_EQ(MapToScore(C::kLsd.poor + 10.0, C::kLsd), 0.0);
  EXPECT_EQ(MapToScore(0.0, C::kLsd), 1.0);
  // More is better.
  EXPECT_EQ(MapToScore(C::kEstoi.good, C::kEstoi), 1.0);
  EXPECT_NEAR(MapToScore(Middle(C::kEstoi), C::kEstoi), 0.5, 1e-12);
  EXPECT_EQ(MapToScore(0.0, C::kEstoi), 0.0);
}

TEST(QualityScoreTest, ComponentsAverageTheirFigures) {
  QualityMeasurement m = At(1.0, 1.0);
  // One envelope figure at poor halves the envelope component.
  m.envelope.mean_db = C::kLsd.poor;
  // One pitch figure at poor takes a third off the pitch component.
  m.pitch.voicing_error = C::kVoicing.poor;
  const QualityComponents c = ScoreComponents(m);
  EXPECT_NEAR(c.envelope, 0.5, 1e-12);
  EXPECT_NEAR(c.pitch, 2.0 / 3.0, 1e-12);
  ASSERT_TRUE(c.intelligibility.has_value());
  EXPECT_NEAR(*c.intelligibility, 1.0, 1e-12);
}

TEST(QualityScoreTest, WeightsSumToOne) {
  EXPECT_NEAR(C::kEnvelopeWeight + C::kPitchWeight + C::kIntelligibilityWeight,
              1.0, 1e-12);
}

TEST(QualityScoreTest, EqualComponentsGiveThatScore) {
  for (const double t : {0.0, 0.25, 0.5, 1.0}) {
    EXPECT_NEAR(ScoreQuality(At(t, t)).overall, t, 1e-12) << t;
  }
}

TEST(QualityScoreTest, APoorComponentPullsTheScoreDown) {
  const QualityComponents good_but_one = {
      .envelope = 1.0, .pitch = 1.0, .intelligibility = 0.1};
  const QualityComponents middling = {
      .envelope = 0.7, .pitch = 0.7, .intelligibility = 0.7};
  // The arithmetic means are 0.7 each; the geometric mean ranks the even
  // encoding above the one with a poor component.
  EXPECT_LT(CombineComponents(good_but_one), CombineComponents(middling));
  EXPECT_NEAR(CombineComponents(good_but_one),
              std::pow(0.1, C::kIntelligibilityWeight), 1e-12);
  EXPECT_EQ(CombineComponents(
                {.envelope = 0.0, .pitch = 1.0, .intelligibility = 1.0}),
            0.0);
}

TEST(QualityScoreTest, WithoutIntelligibilityTheOthersShareItsWeight) {
  const QualityComponents c = {
      .envelope = 0.5, .pitch = 0.5, .intelligibility = std::nullopt};
  EXPECT_NEAR(CombineComponents(c), 0.5, 1e-12);
  const QualityScore s = ScoreQuality(At(1.0, std::nullopt));
  EXPECT_FALSE(s.components.intelligibility.has_value());
  EXPECT_NEAR(s.overall, 1.0, 1e-12);
}

TEST(QualityScoreTest, RatingBandsMeetAtTheirThresholds) {
  EXPECT_EQ(RatingOf(0.0), QualityRating::kPoor);
  EXPECT_EQ(RatingOf(std::nextafter(C::kFairFrom, 0.0)), QualityRating::kPoor);
  EXPECT_EQ(RatingOf(C::kFairFrom), QualityRating::kFair);
  EXPECT_EQ(RatingOf(std::nextafter(C::kGoodFrom, 0.0)), QualityRating::kFair);
  EXPECT_EQ(RatingOf(C::kGoodFrom), QualityRating::kGood);
  EXPECT_EQ(RatingOf(1.0), QualityRating::kGood);
  EXPECT_LT(C::kFairFrom, C::kGoodFrom);
}

TEST(QualityScoreTest, RatingsHaveLabels) {
  EXPECT_EQ(RatingLabel(QualityRating::kPoor), "Poor");
  EXPECT_EQ(RatingLabel(QualityRating::kFair), "Fair");
  EXPECT_EQ(RatingLabel(QualityRating::kGood), "Good");
}

TEST(QualityScoreTest, ScoreRatesItsOverall) {
  const QualityScore s = ScoreQuality(At(1.0, 1.0));
  EXPECT_EQ(s.rating, QualityRating::kGood);
  EXPECT_EQ(ScoreQuality(At(0.0, 0.0)).rating, QualityRating::kPoor);
}

}  // namespace
}  // namespace speak5220::core::quality
