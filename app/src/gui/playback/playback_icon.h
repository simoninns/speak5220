/*
 * File:        playback_icon.h
 * Module:      playback
 * Purpose:     Play and pause icons drawn in the palette's colours
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PLAYBACK_PLAYBACK_ICON_H_
#define SPEAK5220_GUI_PLAYBACK_PLAYBACK_ICON_H_

#include <QIcon>

class QWidget;

namespace speak5220::app {

// The play symbol, or the pause symbol if `pause`, for a button of
// `widget`: drawn in its palette's button text colour, enabled and
// disabled, at the style's small icon size. The style's own media icons
// are fixed black images, so they would vanish in dark mode. Redraw it when the
// palette changes.
QIcon PlayPauseIcon(bool pause, const QWidget& widget);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PLAYBACK_PLAYBACK_ICON_H_
