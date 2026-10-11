/*
 * File:        test_palettes.h
 * Module:      gui tests
 * Purpose:     A light and a dark palette like Qt's Fusion style's, for
 *              tests of the theme's colours
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_GUI_SUPPORT_TEST_PALETTES_H_
#define SPEAK5220_TESTS_GUI_SUPPORT_TEST_PALETTES_H_

#include <QColor>
#include <QPalette>

namespace speak5220::testing {

// A palette with these window, text (and button text) and base colours.
inline QPalette MakePalette(const QColor& window, const QColor& text,
                            const QColor& base) {
  QPalette palette;
  palette.setColor(QPalette::Window, window);
  palette.setColor(QPalette::WindowText, text);
  palette.setColor(QPalette::Base, base);
  palette.setColor(QPalette::Text, text);
  palette.setColor(QPalette::ButtonText, text);
  return palette;
}

// Qt 6's Fusion palettes, light and dark.
inline QPalette LightPalette() {
  return MakePalette(QColor(0xEF, 0xEF, 0xEF), QColor(0x00, 0x00, 0x00),
                     QColor(0xFF, 0xFF, 0xFF));
}

inline QPalette DarkPalette() {
  return MakePalette(QColor(0x32, 0x32, 0x32), QColor(0xFF, 0xFF, 0xFF),
                     QColor(0x1E, 0x1E, 0x1E));
}

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_GUI_SUPPORT_TEST_PALETTES_H_
