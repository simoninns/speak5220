/*
 * File:        colours.h
 * Module:      theme
 * Purpose:     The application's own colours, in a light and a dark form,
 *              chosen for the palette in use
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_THEME_COLOURS_H_
#define SPEAK5220_GUI_THEME_COLOURS_H_

#include <QColor>
#include <QPalette>
#include <array>
#include <cstddef>

namespace speak5220::app {

// The number of bands of the quality rating (core::quality::QualityRating).
inline constexpr std::size_t kQualityBandCount = 3;

// Plan §5, "Colours": the colours the palette has no role for. Widgets
// take everything else from the palette. A plain value type.
struct ThemeColours {
  // A refused name, a phrase too large for a PHROM.
  QColor warning;
  // The waveform: its trace, the zero line, the 25 ms frame grid.
  QColor wave;
  QColor zero_line;
  QColor frame_grid;
  // The playhead and the play-start marker.
  QColor playhead;
  QColor marker;
  // The background of each kind of frame on the encoded waveform.
  QColor voiced;
  QColor unvoiced;
  QColor repeat;
  QColor silence;
  // The quality rating's bands, worst first (core::quality::QualityRating
  // order), and the marks drawn over them. The bands are told apart by
  // their labels, not by colour alone; the marks contrast with every band.
  // Full-strength red, amber and green in both forms, so they read at a
  // glance on a light window as on a dark one.
  std::array<QColor, kQualityBandCount> quality_bands;
  QColor quality_mark;
};

// True when `palette` is dark: its window is darker than its text.
bool IsDarkPalette(const QPalette& palette);

// The colours for `palette`: the dark forms on a dark palette, otherwise
// the light ones. Each contrasts with the palette's base (WCAG 2.1: 4.5:1
// for text, 3:1 for graphics) on Qt's Fusion palettes. Pure.
ThemeColours ColoursFor(const QPalette& palette);

// The palette the waveforms are drawn with: `palette` with a black base
// and window and light text, placeholder text and mid, so a waveform shows
// as in the dark scheme (ColoursFor() gives the dark colours) whichever
// scheme the application is in. Other roles, such as the highlight, are
// kept. Pure.
QPalette WaveformPalette(const QPalette& palette);

// WCAG 2.1 contrast ratio of two colours, from 1 to 21. Pure.
double ContrastRatio(const QColor& a, const QColor& b);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_THEME_COLOURS_H_
