/*
 * File:        colours.cpp
 * Module:      theme
 * Purpose:     The application's own colours, in a light and a dark form,
 *              chosen for the palette in use
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/theme/colours.h"

#include <QColor>
#include <QPalette>
#include <algorithm>
#include <cmath>
#include <optional>
#include <string_view>

namespace speak5220::app {

namespace {

// WCAG 2.1, relative luminance: the sRGB transfer function's linear
// segment and its power segment.
constexpr double kLinearLimit = 0.04045;
constexpr double kLinearSlope = 12.92;
constexpr double kOffset = 0.055;
constexpr double kScale = 1.055;
constexpr double kGamma = 2.4;
constexpr double kRedWeight = 0.2126;
constexpr double kGreenWeight = 0.7152;
constexpr double kBlueWeight = 0.0722;
// WCAG 2.1, contrast ratio: (L1 + 0.05) / (L2 + 0.05).
constexpr double kFlare = 0.05;

double Linear(double channel) {
  return channel <= kLinearLimit
             ? channel / kLinearSlope
             : std::pow((channel + kOffset) / kScale, kGamma);
}

double Luminance(const QColor& colour) {
  return (kRedWeight * Linear(colour.redF())) +
         (kGreenWeight * Linear(colour.greenF())) +
         (kBlueWeight * Linear(colour.blueF()));
}

// The quality rating's bands, worst first, and its marks, the same in
// both forms.
constexpr QColor kPoorBand(0xFF, 0x5A, 0x4A);
constexpr QColor kFairBand(0xFF, 0xC1, 0x07);
constexpr QColor kGoodBand(0x4C, 0xBB, 0x17);
constexpr QColor kQualityMark(0x1F, 0x1F, 0x1F);

// The waveform palette's own roles (WaveformPalette()).
constexpr QColor kWaveformBackground(0x00, 0x00, 0x00);
constexpr QColor kWaveformText(0xFF, 0xFF, 0xFF);
constexpr QColor kWaveformPlaceholder(0x9A, 0x9A, 0x9A);
constexpr QColor kWaveformMid(0x5A, 0x5A, 0x5A);

const ThemeColours kLight = {
    .warning = QColor(0xB3, 0x26, 0x1E),
    .wave = QColor(0x1A, 0x5F, 0xB4),
    .zero_line = QColor(0x8A, 0x8A, 0x8A),
    .frame_grid = QColor(0xB0, 0xB0, 0xB0),
    .playhead = QColor(0x1E, 0x7E, 0x34),
    .marker = QColor(0xC6, 0x46, 0x00),
    .voiced = QColor(0xDD, 0xE8, 0xF7),
    .unvoiced = QColor(0xF8, 0xEA, 0xD2),
    .repeat = QColor(0xDC, 0xEF, 0xDC),
    .silence = QColor(0xEC, 0xEC, 0xEC),
    .quality_bands = {kPoorBand, kFairBand, kGoodBand},
    .quality_mark = kQualityMark,
};

const ThemeColours kDark = {
    .warning = QColor(0xFF, 0x7B, 0x72),
    .wave = QColor(0x78, 0xAE, 0xED),
    .zero_line = QColor(0x8A, 0x8A, 0x8A),
    .frame_grid = QColor(0x5A, 0x5A, 0x5A),
    .playhead = QColor(0x57, 0xE3, 0x89),
    .marker = QColor(0xFF, 0xA3, 0x48),
    .voiced = QColor(0x23, 0x32, 0x47),
    .unvoiced = QColor(0x3D, 0x30, 0x20),
    .repeat = QColor(0x22, 0x38, 0x2A),
    .silence = QColor(0x2E, 0x2E, 0x2E),
    .quality_bands = {kPoorBand, kFairBand, kGoodBand},
    .quality_mark = kQualityMark,
};

}  // namespace

bool IsDarkPalette(const QPalette& palette) {
  return Luminance(palette.color(QPalette::Window)) <
         Luminance(palette.color(QPalette::WindowText));
}

ThemeColours ColoursFor(const QPalette& palette) {
  return IsDarkPalette(palette) ? kDark : kLight;
}

QPalette WaveformPalette(const QPalette& palette) {
  QPalette waveform = palette;
  for (const QPalette::ColorGroup group :
       {QPalette::Active, QPalette::Inactive, QPalette::Disabled}) {
    waveform.setColor(group, QPalette::Base, kWaveformBackground);
    waveform.setColor(group, QPalette::Window, kWaveformBackground);
    waveform.setColor(group, QPalette::Text, kWaveformText);
    waveform.setColor(group, QPalette::WindowText, kWaveformText);
    waveform.setColor(group, QPalette::PlaceholderText, kWaveformPlaceholder);
    waveform.setColor(group, QPalette::Mid, kWaveformMid);
  }
  return waveform;
}

std::string_view WaveformColoursKey(WaveformColours choice) {
  switch (choice) {
    case WaveformColours::kDark:
      return "dark";
    case WaveformColours::kTheme:
      return "theme";
  }
  return "dark";
}

std::optional<WaveformColours> WaveformColoursFromKey(std::string_view key) {
  for (const WaveformColours choice : kWaveformColourChoices) {
    if (WaveformColoursKey(choice) == key) {
      return choice;
    }
  }
  return std::nullopt;
}

QPalette WaveformPaletteFor(const QPalette& palette, WaveformColours choice) {
  return choice == WaveformColours::kDark ? WaveformPalette(palette) : palette;
}

double ContrastRatio(const QColor& a, const QColor& b) {
  const double la = Luminance(a);
  const double lb = Luminance(b);
  return (std::max(la, lb) + kFlare) / (std::min(la, lb) + kFlare);
}

}  // namespace speak5220::app
