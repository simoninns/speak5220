/*
 * File:        quality_rating_view_test.cpp
 * Module:      encoder panel tests
 * Purpose:     Unit tests of the quality rating: the rating and score in
 *              words, the components, Optimise's earlier score, and its
 *              marks without colour
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/quality_rating_view.h"

#include <QColor>
#include <QImage>
#include <QPalette>
#include <QProgressBar>
#include <cstddef>
#include <optional>

#include "core/quality/quality_assessor.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/encoding_summary.h"
#include "gui/support/test_palettes.h"
#include "gui/theme/colours.h"
#include "support/quality_mocks.h"

namespace speak5220::app {
namespace {

TEST(QualityRatingViewTest, ShowsTheRatingScoreComponentsAndFigures) {
  QualityRatingView view;
  view.SetAssessment(testing::AssessmentScoring(0.62));
  EXPECT_EQ(view.Text(), QStringLiteral("Fair · 0.62"));
  ASSERT_TRUE(view.Bar()->Score().has_value());
  EXPECT_NEAR(*view.Bar()->Score(), 0.62, 1e-9);
  for (const auto c : {QualityRatingView::Component::kEnvelope,
                       QualityRatingView::Component::kPitch,
                       QualityRatingView::Component::kIntelligibility}) {
    EXPECT_EQ(view.ComponentBar(c)->format(), QStringLiteral("0.62"));
    EXPECT_EQ(view.ComponentBar(c)->value(), 62);
  }
  EXPECT_TRUE(view.Figures().startsWith(QStringLiteral("envelope ")));
  EXPECT_EQ(view.toolTip(), QualityToolTip());
}

TEST(QualityRatingViewTest, IntelligibilityNotMeasuredIsSaid) {
  core::quality::QualityAssessment a = testing::AssessmentScoring(0.8);
  a.measures.estoi.reset();
  a.score.components.intelligibility.reset();
  QualityRatingView view;
  view.SetAssessment(a);
  EXPECT_EQ(view.ComponentBar(QualityRatingView::Component::kIntelligibility)
                ->format(),
            QStringLiteral("not measured"));
}

TEST(QualityRatingViewTest, MarksAnEarlierScore) {
  QualityRatingView view;
  view.SetScore(0.74, 0.58);
  ASSERT_TRUE(view.Bar()->Before().has_value());
  EXPECT_NEAR(*view.Bar()->Before(), 0.58, 1e-12);
  EXPECT_EQ(view.Text(), QStringLiteral("Good · 0.74 · from 0.58"));
}

TEST(QualityRatingViewTest, WithoutAnAssessmentTheBarIsEmpty) {
  QualityRatingView view;
  view.SetAssessment(testing::AssessmentScoring(0.5));
  view.SetAssessment(std::nullopt);
  EXPECT_FALSE(view.Bar()->Score().has_value());
  EXPECT_EQ(view.Text(), QStringLiteral("Quality not measured"));
  EXPECT_TRUE(view.Figures().isEmpty());
  view.SetPlaceholder(QStringLiteral("Measuring quality..."));
  EXPECT_EQ(view.Text(), QStringLiteral("Measuring quality..."));
}

TEST(QualityRatingViewTest, DrawsOffscreen) {
  QualityRatingView view;
  view.resize(500, 160);
  view.SetAssessment(testing::AssessmentScoring(0.7), 0.5);
  QImage image(view.size(), QImage::Format_ARGB32);
  image.fill(Qt::transparent);
  view.render(&image);
  EXPECT_FALSE(image.isNull());
}

TEST(QualityRatingViewTest, BarStaysShortEnoughToReadAtAGlance) {
  QualityRatingView view;
  view.resize(1200, 160);
  view.show();
  EXPECT_LE(view.Bar()->width(), 520);
  EXPECT_EQ(view.Bar()->maximumWidth(), 520);
}

class QualityRatingColoursTest : public ::testing::TestWithParam<QPalette> {};

// Plan Phase 8: the rating stays readable with colour removed. Each band
// is named in words, and the marks differ in lightness, not just hue,
// from every band (WCAG 2.1's contrast ratio is of luminance alone).
TEST_P(QualityRatingColoursTest, MarksStandOutFromEveryBandWithoutColour) {
  const ThemeColours colours = ColoursFor(GetParam());
  for (const QColor& band : colours.quality_bands) {
    EXPECT_GE(ContrastRatio(colours.quality_mark, band), 4.5)
        << band.name().toStdString();
  }
}

TEST_P(QualityRatingColoursTest, EveryBandHasItsOwnColour) {
  const ThemeColours colours = ColoursFor(GetParam());
  for (std::size_t a = 0; a < colours.quality_bands.size(); ++a) {
    for (std::size_t b = a + 1; b < colours.quality_bands.size(); ++b) {
      EXPECT_NE(colours.quality_bands.at(a), colours.quality_bands.at(b));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(LightAndDark, QualityRatingColoursTest,
                         ::testing::Values(testing::LightPalette(),
                                           testing::DarkPalette()));

TEST(QualityRatingColoursTest, BandsDifferBetweenLightAndDark) {
  const ThemeColours light = ColoursFor(testing::LightPalette());
  const ThemeColours dark = ColoursFor(testing::DarkPalette());
  EXPECT_NE(light.quality_bands, dark.quality_bands);
  EXPECT_NE(light.quality_mark, dark.quality_mark);
}

}  // namespace
}  // namespace speak5220::app
