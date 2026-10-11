/*
 * File:        quality_scale_view_test.cpp
 * Module:      encoder panel tests
 * Purpose:     Unit tests of the quality scale: the score, its band in
 *              words, the tooltip, and its marks without colour
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/quality_scale_view.h"

#include <QColor>
#include <QImage>
#include <QPalette>
#include <cstddef>
#include <optional>

#include "core/quality/quality_scale.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/encoding_summary.h"
#include "gui/support/test_palettes.h"
#include "gui/theme/colours.h"

namespace speak5220::app {
namespace {

using core::quality::QualityBand;

TEST(QualityScaleViewTest, ShowsTheScoreItsAnchorAndItsBand) {
  QualityScaleView view;
  view.SetScores(2.08, 2.31);
  EXPECT_EQ(view.Text(),
            QStringLiteral("PESQ 2.08 (LPC best for this phrase 2.31): Good"));
  ASSERT_TRUE(view.Placement().has_value());
  EXPECT_EQ(view.Placement()->band, QualityBand::kGood);
  ASSERT_TRUE(view.Bar()->Position().has_value());
  EXPECT_NEAR(*view.Bar()->Position(), (2.08 - 1.0) / (2.31 - 1.0), 1e-12);
  EXPECT_EQ(view.toolTip(), QualityToolTip());
}

TEST(QualityScaleViewTest, ScoreAtOrAboveTheAnchorIsAtTheTop) {
  QualityScaleView view;
  view.SetScores(2.5, 2.31);
  ASSERT_TRUE(view.Placement().has_value());
  EXPECT_EQ(view.Placement()->band, QualityBand::kAsGoodAsLpc);
  EXPECT_DOUBLE_EQ(*view.Bar()->Position(), 1.0);
  EXPECT_TRUE(
      view.Text().endsWith(QStringLiteral("As good as LPC can achieve")));
}

TEST(QualityScaleViewTest, MarksAnEarlierScore) {
  QualityScaleView view;
  view.SetScores(2.2, 2.5, 1.9);
  ASSERT_TRUE(view.Bar()->Before().has_value());
  EXPECT_NEAR(*view.Bar()->Before(), 0.6, 1e-12);
  EXPECT_TRUE(view.Text().endsWith(QStringLiteral(" · from 1.90")));
}

TEST(QualityScaleViewTest, WithoutAScoreTheBarIsEmpty) {
  QualityScaleView view;
  view.SetScores(2.0, 2.5);
  view.SetScores(std::nullopt, 2.5);
  EXPECT_FALSE(view.Bar()->Position().has_value());
  EXPECT_EQ(view.Text(), QStringLiteral("Quality not measured"));
  view.SetPlaceholder(QStringLiteral("Measuring quality..."));
  EXPECT_EQ(view.Text(), QStringLiteral("Measuring quality..."));
  EXPECT_FALSE(view.Placement().has_value());
}

TEST(QualityScaleViewTest, DrawsOffscreen) {
  QualityScaleView view;
  view.resize(500, 60);
  view.SetScores(2.08, 2.31, 1.9);
  QImage image(view.size(), QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  view.render(&image);
  EXPECT_FALSE(image.isNull());
}

TEST(QualityScaleViewTest, BarStaysShortEnoughToReadAtAGlance) {
  QualityScaleView view;
  view.resize(1200, 60);
  view.show();
  EXPECT_LE(view.Bar()->width(), 520);
  EXPECT_EQ(view.Bar()->maximumWidth(), 520);
}

TEST(QualityScaleViewTest, NamesTheTopBandBestInsideTheBar) {
  EXPECT_EQ(BandName(QualityBand::kAsGoodAsLpc), "Best");
  EXPECT_EQ(BandName(QualityBand::kGood), "Good");
}

class QualityScaleColoursTest : public ::testing::TestWithParam<QPalette> {};

// Plan Phase 7: the scale stays readable with colour removed. The band is
// named in words, and the marks differ in lightness, not just hue, from
// every band (WCAG 2.1's contrast ratio is of luminance alone).
TEST_P(QualityScaleColoursTest, MarksStandOutFromEveryBandWithoutColour) {
  const ThemeColours colours = ColoursFor(GetParam());
  for (const QColor& band : colours.quality_bands) {
    EXPECT_GE(ContrastRatio(colours.quality_mark, band), 4.5)
        << band.name().toStdString();
  }
}

INSTANTIATE_TEST_SUITE_P(LightAndDark, QualityScaleColoursTest,
                         ::testing::Values(testing::LightPalette(),
                                           testing::DarkPalette()));

TEST_P(QualityScaleColoursTest, EveryBandHasItsOwnColour) {
  const ThemeColours colours = ColoursFor(GetParam());
  for (std::size_t a = 0; a < colours.quality_bands.size(); ++a) {
    for (std::size_t b = a + 1; b < colours.quality_bands.size(); ++b) {
      EXPECT_NE(colours.quality_bands.at(a), colours.quality_bands.at(b));
    }
  }
}

TEST(QualityScaleColoursTest, BandsDifferBetweenLightAndDark) {
  const ThemeColours light = ColoursFor(testing::LightPalette());
  const ThemeColours dark = ColoursFor(testing::DarkPalette());
  EXPECT_NE(light.quality_bands, dark.quality_bands);
  EXPECT_NE(light.quality_mark, dark.quality_mark);
}

}  // namespace
}  // namespace speak5220::app
