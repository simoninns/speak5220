/*
 * File:        colours_test.cpp
 * Module:      theme tests
 * Purpose:     Unit tests of the theme's colours: light and dark forms, and
 *              their contrast with the background
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/theme/colours.h"

#include <QColor>
#include <QPalette>
#include <vector>

#include "gtest/gtest.h"
#include "gui/support/test_palettes.h"

namespace speak5220::app {
namespace {

using testing::DarkPalette;
using testing::LightPalette;

// WCAG 2.1: 4.5:1 for text, 3:1 for graphics.
constexpr double kTextContrast = 4.5;
constexpr double kGraphicsContrast = 3.0;

TEST(ColoursTest, ContrastRatioRunsFromOneToTwentyOne) {
  EXPECT_NEAR(ContrastRatio(Qt::black, Qt::white), 21.0, 1e-9);
  EXPECT_NEAR(ContrastRatio(Qt::white, Qt::black), 21.0, 1e-9);
  EXPECT_NEAR(ContrastRatio(QColor(0x77, 0x77, 0x77), QColor(0x77, 0x77, 0x77)),
              1.0, 1e-9);
  // WCAG's own example: #777 on white is just under 4.5:1.
  EXPECT_NEAR(ContrastRatio(QColor(0x77, 0x77, 0x77), Qt::white), 4.48, 0.01);
}

TEST(ColoursTest, TellsDarkPalettesFromLight) {
  EXPECT_FALSE(IsDarkPalette(LightPalette()));
  EXPECT_TRUE(IsDarkPalette(DarkPalette()));
}

TEST(ColoursTest, DarkPaletteGetsOtherColours) {
  const ThemeColours light = ColoursFor(LightPalette());
  const ThemeColours dark = ColoursFor(DarkPalette());
  EXPECT_NE(light.warning, dark.warning);
  EXPECT_NE(light.wave, dark.wave);
  EXPECT_NE(light.playhead, dark.playhead);
  EXPECT_NE(light.marker, dark.marker);
  EXPECT_NE(light.voiced, dark.voiced);
}

TEST(ColoursTest, WaveformPaletteIsDarkOnBlackInEitherScheme) {
  for (const QPalette& palette : {LightPalette(), DarkPalette()}) {
    const QPalette waveform = WaveformPalette(palette);
    EXPECT_TRUE(IsDarkPalette(waveform));
    EXPECT_EQ(waveform.color(QPalette::Base), QColor(Qt::black));
    EXPECT_GE(ContrastRatio(waveform.color(QPalette::PlaceholderText),
                            waveform.color(QPalette::Base)),
              kTextContrast);
    EXPECT_EQ(waveform.color(QPalette::Highlight),
              palette.color(QPalette::Highlight));
  }
  EXPECT_EQ(ColoursFor(WaveformPalette(LightPalette())).wave,
            ColoursFor(DarkPalette()).wave);
}

class ColoursContrastTest : public ::testing::TestWithParam<QPalette> {};

TEST_P(ColoursContrastTest, WarningIsReadableTextOnTheBackground) {
  const QPalette& palette = GetParam();
  const ThemeColours colours = ColoursFor(palette);
  EXPECT_GE(ContrastRatio(colours.warning, palette.color(QPalette::Base)),
            kTextContrast);
  EXPECT_GE(ContrastRatio(colours.warning, palette.color(QPalette::Window)),
            kTextContrast);
}

TEST_P(ColoursContrastTest, MarksStandOutFromTheBackground) {
  const QPalette& palette = GetParam();
  const ThemeColours colours = ColoursFor(palette);
  const QColor base = palette.color(QPalette::Base);
  for (const QColor& mark :
       {colours.wave, colours.zero_line, colours.playhead, colours.marker}) {
    EXPECT_GE(ContrastRatio(mark, base), kGraphicsContrast)
        << mark.name().toStdString();
  }
}

TEST_P(ColoursContrastTest, WaveStandsOutFromEveryKindOfFrame) {
  const ThemeColours colours = ColoursFor(GetParam());
  for (const QColor& fill :
       {colours.voiced, colours.unvoiced, colours.repeat, colours.silence}) {
    EXPECT_GE(ContrastRatio(colours.wave, fill), kGraphicsContrast)
        << fill.name().toStdString();
    EXPECT_GE(ContrastRatio(colours.playhead, fill), kGraphicsContrast)
        << fill.name().toStdString();
    EXPECT_GE(ContrastRatio(colours.marker, fill), kGraphicsContrast)
        << fill.name().toStdString();
  }
}

INSTANTIATE_TEST_SUITE_P(LightAndDark, ColoursContrastTest,
                         ::testing::Values(LightPalette(), DarkPalette()));

}  // namespace
}  // namespace speak5220::app
