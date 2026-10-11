/*
 * File:        quality_scale_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the quality scale: position, clamping, bands
 *              and labels
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

TEST(QualityScaleTest, PositionRunsFromTheFloorToTheAnchor) {
  EXPECT_DOUBLE_EQ(QualityScale::Place(1.0, 2.5).position, 0.0);
  EXPECT_DOUBLE_EQ(QualityScale::Place(2.5, 2.5).position, 1.0);
  EXPECT_DOUBLE_EQ(QualityScale::Place(2.0, 3.0).position, 0.5);
}

TEST(QualityScaleTest, PositionIsClampedToTheScale) {
  EXPECT_EQ(QualityScale::Place(2.9, 2.5).position, 1.0);
  EXPECT_EQ(QualityScale::Place(0.8, 2.5).position, 0.0);
  EXPECT_EQ(QualityScale::Place(2.9, 2.5).band, QualityBand::kAsGoodAsLpc);
}

TEST(QualityScaleTest, AnchorAtTheFloorLeavesNoScale) {
  EXPECT_EQ(QualityScale::Place(1.0, 1.0).position, 1.0);
  EXPECT_EQ(QualityScale::Place(0.9, 1.0).position, 0.0);
  EXPECT_EQ(QualityScale::Place(1.2, 0.95).position, 1.0);
}

TEST(QualityScaleTest, BandsStartAtTheirThresholds) {
  EXPECT_EQ(QualityScale::BandOf(0.0), QualityBand::kBad);
  EXPECT_EQ(QualityScale::BandOf(0.3999), QualityBand::kBad);
  EXPECT_EQ(QualityScale::BandOf(0.40), QualityBand::kPoor);
  EXPECT_EQ(QualityScale::BandOf(0.5999), QualityBand::kPoor);
  EXPECT_EQ(QualityScale::BandOf(0.60), QualityBand::kFair);
  EXPECT_EQ(QualityScale::BandOf(0.7499), QualityBand::kFair);
  EXPECT_EQ(QualityScale::BandOf(0.75), QualityBand::kGood);
  EXPECT_EQ(QualityScale::BandOf(0.8999), QualityBand::kGood);
  EXPECT_EQ(QualityScale::BandOf(0.90), QualityBand::kAsGoodAsLpc);
  EXPECT_EQ(QualityScale::BandOf(1.0), QualityBand::kAsGoodAsLpc);
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
        QualityBand::kGood, QualityBand::kAsGoodAsLpc}) {
    labels.emplace(QualityScale::Label(b));
  }
  EXPECT_EQ(labels.size(), 5U);
  EXPECT_EQ(QualityScale::Label(QualityBand::kAsGoodAsLpc),
            "As good as LPC can achieve");
}

}  // namespace
}  // namespace speak5220::core::quality
