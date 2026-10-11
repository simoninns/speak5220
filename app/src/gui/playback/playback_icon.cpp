/*
 * File:        playback_icon.cpp
 * Module:      playback
 * Purpose:     Play and pause icons drawn in the palette's colours
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/playback/playback_icon.h"

#include <QColor>
#include <QIcon>
#include <QPainter>
#include <QPalette>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSize>
#include <QStyle>
#include <QWidget>

namespace speak5220::app {

namespace {

// The play or pause symbol in `colour`, `size` device-independent pixels
// square.
QPixmap PlaybackPixmap(bool pause, const QColor& colour, int size,
                       qreal ratio) {
  // The symbol's place within the square, as fractions of its side.
  constexpr qreal kLeft = 0.25;
  constexpr qreal kRight = 0.8;
  constexpr qreal kTop = 0.15;
  constexpr qreal kBottom = 0.85;
  constexpr qreal kBar = 0.18;
  QPixmap pixmap(QSize(size, size) * ratio);
  pixmap.setDevicePixelRatio(ratio);
  pixmap.fill(Qt::transparent);
  QPainter painter(&pixmap);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(colour);
  const qreal side = size;
  if (pause) {
    painter.drawRect(QRectF(kLeft * side, kTop * side, kBar * side,
                            (kBottom - kTop) * side));
    painter.drawRect(QRectF((kRight - kBar) * side, kTop * side, kBar * side,
                            (kBottom - kTop) * side));
  } else {
    QPolygonF triangle;
    triangle << QPointF(kLeft * side, kTop * side)
             << QPointF(kRight * side, side / 2.0)
             << QPointF(kLeft * side, kBottom * side);
    painter.drawPolygon(triangle);
  }
  return pixmap;
}

}  // namespace

QIcon PlayPauseIcon(bool pause, const QWidget& widget) {
  const int size = widget.style()->pixelMetric(QStyle::PM_SmallIconSize);
  const qreal ratio = widget.devicePixelRatioF();
  const QPalette& palette = widget.palette();
  QIcon icon;
  icon.addPixmap(
      PlaybackPixmap(pause,
                     palette.color(QPalette::Active, QPalette::ButtonText),
                     size, ratio),
      QIcon::Normal);
  icon.addPixmap(
      PlaybackPixmap(pause,
                     palette.color(QPalette::Disabled, QPalette::ButtonText),
                     size, ratio),
      QIcon::Disabled);
  return icon;
}

}  // namespace speak5220::app
