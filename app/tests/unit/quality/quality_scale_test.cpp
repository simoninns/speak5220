/*
 * File:        quality_scale_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the quality scale: relative score, position,
 *              clamping, bands and labels
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/quality_scale.h"

#include <set>
#include <string>

#include "gtest/gtest.h"

namespace speak5220::core::quality {
namespace {

TEST(QualityScaleTest, RelativeScoreRunsFromTheFloorToTheAnchor) {
  EXPECT_DOUBLE_EQ(QualityScale::Place(1.0, 2.5).relative, 0.0);
  EXPECT_DOUBLE_EQ(QualityScale::Place(2.5, 2.5).relative, 1.0);
  EXPECT_DOUBLE_EQ(QualityScale::Place(2.0, 3.0).relative, 0.5);
  // Beyond the anchor: (2.2 - 1) / (2.0 - 1).
  EXPECT_DOUBLE_EQ(QualityScale::Place(2.2, 2.0).relative, 1.2);
}

TEST(QualityScaleTest, BarRunsPastTheAnchorToTheTop) {
  EXPECT_DOUBLE_EQ(QualityScale::Place(1.0, 2.5).position, 0.0);
  EXPECT_DOUBLE_EQ(QualityScale::Place(2.0, 2.0).position,
                   1.0 / QualityScale::kTop);
  EXPECT_DOUBLE_EQ(QualityScale::Place(2.25, 2.0).position, 1.0);
  // A score beyond the anchor still shows how far.
  EXPECT_LT(QualityScale::Place(2.1, 2.0).position,
            QualityScale::Place(2.2, 2.0).position);
}

TEST(QualityScaleTest, PositionIsClampedToTheBar) {
  EXPECT_EQ(QualityScale::Place(2.9, 2.0).position, 1.0);
  EXPECT_EQ(QualityScale::Place(0.8, 2.5).position, 0.0);
  EXPECT_EQ(QualityScale::Place(2.9, 2.0).band, QualityBand::kLpcOrBetter);
}

TEST(QualityScaleTest, AnchorAtTheFloorLeavesNoScale) {
  EXPECT_EQ(QualityScale::Place(1.0, 1.0).relative, 1.0);
  EXPECT_EQ(QualityScale::Place(0.9, 1.0).relative, 0.0);
  EXPECT_EQ(QualityScale::Place(1.2, 0.95).relative, 1.0);
}

TEST(QualityScaleTest, BandsStartAtTheirThresholds) {
  EXPECT_EQ(QualityScale::BandOf(0.0), QualityBand::kBad);
  EXPECT_EQ(QualityScale::BandOf(0.3999), QualityBand::kBad);
  EXPECT_EQ(QualityScale::BandOf(0.40), QualityBand::kPoor);
  EXPECT_EQ(QualityScale::BandOf(0.5999), QualityBand::kPoor);
  EXPECT_EQ(QualityScale::BandOf(0.60), QualityBand::kFair);
  EXPECT_EQ(QualityScale::BandOf(0.7499), QualityBand::kFair);
  EXPECT_EQ(QualityScale::BandOf(0.75), QualityBand::kGood);
  EXPECT_EQ(QualityScale::BandOf(0.9999), QualityBand::kGood);
  EXPECT_EQ(QualityScale::BandOf(1.0), QualityBand::kLpcOrBetter);
  EXPECT_EQ(QualityScale::BandOf(1.4), QualityBand::kLpcOrBetter);
}

TEST(QualityScaleTest, PlacementCarriesItsBand) {
  // (1.8 - 1) / (2.0 - 1) = 0.8.
  EXPECT_EQ(QualityScale::Place(1.8, 2.0).band, QualityBand::kGood);
  // (1.5 - 1) / (2.0 - 1) = 0.5.
  EXPECT_EQ(QualityScale::Place(1.5, 2.0).band, QualityBand::kPoor);
}

TEST(QualityScaleTest, EveryBandHasItsOwnLabel) {
  std::set<std::string> labels;
  for (QualityBand b :
       {QualityBand::kBad, QualityBand::kPoor, QualityBand::kFair,
        QualityBand::kGood, QualityBand::kLpcOrBetter}) {
    labels.emplace(QualityScale::Label(b));
  }
  EXPECT_EQ(labels.size(), 5U);
  EXPECT_EQ(QualityScale::Label(QualityBand::kLpcOrBetter),
            "As good as LPC or better");
}

}  // namespace
}  // namespace speak5220::core::quality
