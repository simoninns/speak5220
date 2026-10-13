/*
 * File:        grip_splitter.cpp
 * Module:      waveform
 * Purpose:     A splitter whose handle shows a grip, so it can be found on
 *              every platform's style
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/grip_splitter.h"

#include <QColor>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPointF>
#include <QRectF>
#include <QSizeF>
#include <QSplitterHandle>
#include <algorithm>

namespace speak5220::app {
namespace {

// The handle's thickness, wide enough to take hold of, in pixels.
constexpr int kHandleWidth = 8;
// The grip bar's length along the handle, and its thickness, in pixels.
constexpr double kGripLength = 36.0;
constexpr double kGripThickness = 3.0;
// The grip's opacity in the text colour when the pointer is elsewhere.
constexpr float kGripOpacity = 0.45F;

// A splitter handle that draws the grip over the style's handle.
class GripHandle : public QSplitterHandle {
 public:
  GripHandle(Qt::Orientation orientation, QSplitter* parent)
      : QSplitterHandle(orientation, parent) {
    // Repaints as the pointer enters and leaves, for the highlight
    setAttribute(Qt::WA_Hover);
  }

 protected:
  void paintEvent(QPaintEvent* event) override {
    QSplitterHandle::paintEvent(event);
    QColor colour;
    if (underMouse()) {
      colour = palette().color(QPalette::Highlight);
    } else {
      colour = palette().color(QPalette::WindowText);
      colour.setAlphaF(kGripOpacity);
    }
    // A vertical splitter stacks its widgets, so its handle runs across
    // the window and the bar lies along it.
    const bool across = orientation() == Qt::Vertical;
    const auto along = static_cast<double>(across ? width() : height());
    const auto deep = static_cast<double>(across ? height() : width());
    const double length = std::min(kGripLength, along);
    const double thickness = std::min(kGripThickness, deep);
    const QSizeF size =
        across ? QSizeF(length, thickness) : QSizeF(thickness, length);
    QRectF bar(QPointF(0.0, 0.0), size);
    bar.moveCenter(QRectF(rect()).center());

    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colour);
    painter.drawRoundedRect(bar, thickness / 2.0, thickness / 2.0);
  }
};

}  // namespace

GripSplitter::GripSplitter(Qt::Orientation orientation, QWidget* parent)
    : QSplitter(orientation, parent) {
  setHandleWidth(kHandleWidth);
}

QSplitterHandle* GripSplitter::createHandle() {
  return new GripHandle(orientation(), this);
}

}  // namespace speak5220::app
