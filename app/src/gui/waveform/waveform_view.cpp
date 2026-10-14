/*
 * File:        waveform_view.cpp
 * Module:      waveform
 * Purpose:     Draws a waveform with its play-start marker and playhead,
 *              and the frames over the encoded audio; zoom and scroll
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/waveform_view.h"

#include <QApplication>
#include <QBrush>
#include <QColor>
#include <QEvent>
#include <QHelpEvent>
#include <QKeyEvent>
#include <QLineF>
#include <QMouseEvent>
#include <QPaintEvent>
#include <QPainter>
#include <QPalette>
#include <QPen>
#include <QPixmap>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QResizeEvent>
#include <QSize>
#include <QSizeF>
#include <QString>
#include <QToolTip>
#include <QWheelEvent>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "gui/theme/colours.h"
#include "gui/waveform/frame_map.h"
#include "gui/waveform/peak_cache.h"
#include "gui/waveform/time_axis.h"

namespace speak5220::app {

namespace lpc = core::lpc;

namespace {

// The view's preferred and smallest size, in pixels.
constexpr int kPreferredWidth = 400;
constexpr int kPreferredHeight = 160;
constexpr int kMinimumWidth = 120;
constexpr int kMinimumHeight = 60;
// Space above and below the waveform's full scale, in pixels.
constexpr double kVerticalMargin = 4.0;
// Below this many samples per pixel the samples are joined by lines; above
// it each pixel is drawn from the peaks of its samples.
constexpr double kJoinedSamplesPerPixel = 4.0;
// The marker's flag at the top of the view, in pixels.
constexpr double kMarkerFlag = 6.0;
// One wheel notch (QWheelEvent::angleDelta()) and how far it scrolls.
constexpr double kWheelNotch = 120.0;
constexpr double kScrollPixelsPerNotch = 40.0;
// How much of the background veils a shaded stretch's waveform: enough to
// dim it, not to hide it.
constexpr int kShadeAlpha = 170;
// How strongly the selection is tinted: enough to see, not to hide the
// waveform under it.
constexpr int kSelectionAlpha = 90;
// A selection whose fit is less than this much closer than the view's
// zoom is shown already, so zooming in steps instead of fitting it.
constexpr double kSelectionFitTolerance = 1.01;
// The backdrop's opacity, enough to be seen without drawing the eye, and
// its largest share of the view's width and height.
constexpr double kBackdropOpacity = 0.15;
constexpr double kBackdropShare = 0.6;
// The gap between the backdrop and the placeholder under it, in pixels.
constexpr int kBackdropGap = 8;

}  // namespace

WaveformView::WaveformView(QWidget* parent)
    : QWidget(parent),
      wave_palette_(WaveformPalette(palette())),
      colours_(ColoursFor(wave_palette_)) {
  setFocusPolicy(Qt::StrongFocus);
  setMouseTracking(true);
  setAutoFillBackground(false);
}

void WaveformView::SetWaveformColours(WaveformColours choice) {
  colour_choice_ = choice;
  wave_palette_ = WaveformPaletteFor(palette(), colour_choice_);
  colours_ = ColoursFor(wave_palette_);
  update();
}

void WaveformView::SetSamples(std::vector<float> samples, int source_offset) {
  peaks_ = PeakCache(std::move(samples));
  source_offset_ = source_offset;
  placeholder_.clear();
  backdrop_ = QPixmap();
  extent_ = ContentExtent();
  Fit();
  update();
}

void WaveformView::Clear(const QString& placeholder, QPixmap backdrop) {
  peaks_ = PeakCache();
  source_offset_ = 0;
  frames_.clear();
  shading_.clear();
  placeholder_ = placeholder;
  backdrop_ = std::move(backdrop);
  playhead_.reset();
  update();
}

void WaveformView::SetShading(std::vector<TimeExtent> spans) {
  if (spans != shading_) {
    shading_ = std::move(spans);
    update();
  }
}

void WaveformView::SetFrames(std::vector<lpc::Frame> frames) {
  frames_ = std::move(frames);
  update();
}

void WaveformView::SetFrameGridShown(bool shown) {
  frame_grid_ = shown;
  update();
}

TimeExtent WaveformView::ContentExtent() const {
  return {.begin = static_cast<double>(-source_offset_),
          .end = static_cast<double>(peaks_.Size()) - source_offset_};
}

void WaveformView::SetExtent(const TimeExtent& extent) {
  extent_ = extent;
  Apply(fitted_ ? FitAxis(extent_, width()) : axis_, fitted_);
}

void WaveformView::SetAxis(const TimeAxis& axis) {
  Apply(axis, ClampAxis(axis, extent_, width()) == FitAxis(extent_, width()));
}

void WaveformView::ZoomIn() {
  // The first zoom in shows just the selection, if the view shows more.
  if (selection_.has_value()) {
    const TimeAxis selected =
        ClampAxis(FitAxis(*selection_, width()), extent_, width());
    if (selected.samples_per_pixel <
        axis_.samples_per_pixel / kSelectionFitTolerance) {
      Apply(selected, selected == FitAxis(extent_, width()));
      return;
    }
  }
  ZoomBy(kZoomStep);
}

void WaveformView::ZoomOut() { ZoomBy(1.0 / kZoomStep); }

void WaveformView::ZoomBy(double factor) {
  double centre = axis_.XToSample(width() / 2.0);
  if (selection_.has_value()) {
    centre = (selection_->begin + selection_->end) / 2.0;
  } else if (marker_.has_value()) {
    centre = *marker_;
  }
  const TimeAxis zoomed =
      ZoomAxisCentred(axis_, factor, centre, extent_, width());
  Apply(zoomed, zoomed == FitAxis(extent_, width()));
}

void WaveformView::Fit() { Apply(FitAxis(extent_, width()), true); }

void WaveformView::ScrollBy(double pixels) {
  Apply(ScrollAxis(axis_, pixels, extent_, width()), false);
}

void WaveformView::SetMarker(std::optional<int> source_sample) {
  if (marker_ != source_sample) {
    marker_ = source_sample;
    update();
  }
}

void WaveformView::SetSelection(std::optional<TimeExtent> selection) {
  if (selection_ != selection) {
    selection_ = selection;
    update();
  }
}

void WaveformView::SetPlayhead(std::optional<double> source_sample) {
  if (playhead_ != source_sample) {
    playhead_ = source_sample;
    update();
  }
}

double WaveformView::XOfSample(double index) const {
  return axis_.SampleToX(index - source_offset_);
}

std::optional<int> WaveformView::FrameAtX(double x) const {
  if (frames_.empty()) {
    return std::nullopt;
  }
  const double sample = axis_.XToSample(x) + source_offset_;
  return FrameAtEncodedSample(static_cast<std::int64_t>(std::floor(sample)),
                              static_cast<int>(frames_.size()));
}

QString WaveformView::ToolTipAt(double x) const {
  if (const std::optional<int> frame = FrameAtX(x); frame.has_value()) {
    return DescribeFrame(*frame, frames_[static_cast<std::size_t>(*frame)]);
  }
  const double sample = axis_.XToSample(x);
  if (std::ranges::any_of(shading_, [sample](const TimeExtent& span) {
        return sample >= span.begin && sample < span.end;
      })) {
    return QStringLiteral(
        "Gated: turned down by the noise gate before encoding and the "
        "quality measure");
  }
  return {};
}

QSize WaveformView::sizeHint() const {
  return {kPreferredWidth, kPreferredHeight};
}

QSize WaveformView::minimumSizeHint() const {
  return {kMinimumWidth, kMinimumHeight};
}

bool WaveformView::event(QEvent* event) {
  auto* help = dynamic_cast<QHelpEvent*>(event);
  if (event->type() == QEvent::ToolTip && help != nullptr) {
    const QString text = ToolTipAt(help->pos().x());
    if (text.isEmpty()) {
      QToolTip::hideText();
      event->ignore();
    } else {
      QToolTip::showText(help->globalPos(), text, this);
    }
    return true;
  }
  return QWidget::event(event);
}

void WaveformView::changeEvent(QEvent* event) {
  // Plan §5, "Colours": a change of scheme reaches every widget as a
  // palette change. A dark waveform stays dark, keeping only the new
  // palette's other roles.
  if (event->type() == QEvent::PaletteChange) {
    wave_palette_ = WaveformPaletteFor(palette(), colour_choice_);
    colours_ = ColoursFor(wave_palette_);
    update();
  }
  QWidget::changeEvent(event);
}

void WaveformView::keyPressEvent(QKeyEvent* event) {
  if (event->key() == Qt::Key_Space && event->modifiers() == Qt::NoModifier) {
    emit PlayPauseRequested();
    event->accept();
    return;
  }
  if (event->key() == Qt::Key_Escape && selection_.has_value()) {
    emit SelectionClearRequested();
    event->accept();
    return;
  }
  QWidget::keyPressEvent(event);
}

void WaveformView::mousePressEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && HasSamples()) {
    // A click or a drag: which, the release or a move will tell.
    setFocus(Qt::MouseFocusReason);
    press_x_ = event->position().x();
    dragging_ = false;
    event->accept();
    return;
  }
  QWidget::mousePressEvent(event);
}

void WaveformView::mouseMoveEvent(QMouseEvent* event) {
  if (press_x_.has_value() && (event->buttons() & Qt::LeftButton) != 0) {
    const double x = event->position().x();
    if (!dragging_ &&
        std::abs(x - *press_x_) >= QApplication::startDragDistance()) {
      dragging_ = true;
    }
    if (dragging_) {
      RequestSelectionTo(x, false);
    }
    event->accept();
    return;
  }
  QWidget::mouseMoveEvent(event);
}

void WaveformView::mouseReleaseEvent(QMouseEvent* event) {
  if (event->button() == Qt::LeftButton && press_x_.has_value()) {
    const double x = event->position().x();
    if (dragging_) {
      RequestSelectionTo(x, true);
    } else if (HasSamples()) {
      emit MarkerRequested(axis_.XToSample(x));
    }
    press_x_.reset();
    dragging_ = false;
    event->accept();
    return;
  }
  QWidget::mouseReleaseEvent(event);
}

void WaveformView::RequestSelectionTo(double x, bool finished) {
  if (!HasSamples() || !press_x_.has_value()) {
    return;
  }
  const double from = axis_.XToSample(*press_x_);
  const double to = axis_.XToSample(x);
  emit SelectionRequested(std::min(from, to), std::max(from, to), finished);
}

void WaveformView::resizeEvent(QResizeEvent* event) {
  QWidget::resizeEvent(event);
  const TimeAxis before = axis_;
  Apply(fitted_ ? FitAxis(extent_, width()) : axis_, fitted_);
  if (axis_ == before) {
    // The width changed even if the axis did not.
    emit AxisChanged();
  }
}

void WaveformView::wheelEvent(QWheelEvent* event) {
  const QPoint delta = event->angleDelta();
  const double notches =
      static_cast<double>(delta.y() != 0 ? delta.y() : delta.x()) / kWheelNotch;
  if ((event->modifiers() & Qt::ControlModifier) != 0) {
    const TimeAxis zoomed = ZoomAxis(axis_, std::pow(kZoomStep, notches),
                                     event->position().x(), extent_, width());
    Apply(zoomed, zoomed == FitAxis(extent_, width()));
  } else {
    // Wheel down, or right, moves later.
    ScrollBy(-notches * kScrollPixelsPerNotch);
  }
  event->accept();
}

void WaveformView::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.fillRect(rect(), wave_palette_.base());
  if (!HasSamples()) {
    if (HasBackdrop()) {
      PaintBackdrop(painter);
      return;
    }
    painter.setPen(wave_palette_.color(QPalette::PlaceholderText));
    painter.drawText(rect(), Qt::AlignCenter | Qt::TextWordWrap, placeholder_);
    return;
  }
  PaintFrames(painter);
  painter.setPen(QPen(colours_.zero_line, 0));
  painter.drawLine(QLineF(0.0, SampleY(0.0F), width(), SampleY(0.0F)));
  PaintWave(painter);
  PaintShading(painter);
  PaintSelection(painter);
  PaintMarks(painter);
  if (hasFocus()) {
    painter.setPen(QPen(wave_palette_.color(QPalette::Highlight), 0));
    painter.drawRect(rect().adjusted(0, 0, -1, -1));
  }
}

void WaveformView::PaintBackdrop(QPainter& painter) const {
  // Its own aspect, as large as fits the share of the view but no larger
  // than its own size, so it is never blurred by enlarging; centred.
  const QSizeF natural = backdrop_.deviceIndependentSize();
  const QSizeF size =
      natural.scaled(std::min(width() * kBackdropShare, natural.width()),
                     std::min(height() * kBackdropShare, natural.height()),
                     Qt::KeepAspectRatio);
  const QRectF target(
      QPointF((width() - size.width()) / 2.0, (height() - size.height()) / 2.0),
      size);
  painter.save();
  painter.setRenderHint(QPainter::SmoothPixmapTransform);
  painter.setOpacity(kBackdropOpacity);
  painter.drawPixmap(target, backdrop_, QRectF(backdrop_.rect()));
  painter.restore();
  painter.setPen(wave_palette_.color(QPalette::PlaceholderText));
  const QRectF below(0.0, target.bottom() + kBackdropGap, width(),
                     height() - target.bottom() - kBackdropGap);
  painter.drawText(below, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap,
                   placeholder_);
}

void WaveformView::Apply(const TimeAxis& axis, bool fitted) {
  const TimeAxis clamped = ClampAxis(axis, extent_, width());
  fitted_ = fitted;
  if (clamped != axis_) {
    axis_ = clamped;
    update();
    emit AxisChanged();
  }
}

void WaveformView::PaintShading(QPainter& painter) const {
  QColor veil = wave_palette_.color(QPalette::Base);
  veil.setAlpha(kShadeAlpha);
  // Hatched as well as dimmed, so it reads without colour.
  const QBrush hatch(wave_palette_.color(QPalette::Mid), Qt::BDiagPattern);
  for (const TimeExtent& span : shading_) {
    const double left = std::max(axis_.SampleToX(span.begin), 0.0);
    const double right =
        std::min(axis_.SampleToX(span.end), static_cast<double>(width()));
    if (right <= left) {
      continue;
    }
    const QRectF stretch(left, 0.0, right - left, height());
    painter.fillRect(stretch, veil);
    painter.fillRect(stretch, hatch);
  }
}

void WaveformView::PaintSelection(QPainter& painter) const {
  if (!selection_.has_value()) {
    return;
  }
  const double left = std::max(axis_.SampleToX(selection_->begin), 0.0);
  const double right =
      std::min(axis_.SampleToX(selection_->end), static_cast<double>(width()));
  if (right <= left) {
    return;
  }
  // Tinted, with edges, so the waveform shows through and the ends read
  // without colour.
  QColor tint = wave_palette_.color(QPalette::Highlight);
  tint.setAlpha(kSelectionAlpha);
  painter.fillRect(QRectF(left, 0.0, right - left, height()), tint);
  painter.setPen(QPen(wave_palette_.color(QPalette::Highlight), 0));
  for (const double edge :
       {axis_.SampleToX(selection_->begin), axis_.SampleToX(selection_->end)}) {
    if (edge >= 0.0 && edge <= width()) {
      painter.drawLine(QLineF(edge, 0.0, edge, height()));
    }
  }
}

double WaveformView::SampleY(float value) const {
  const double middle = height() / 2.0;
  const double scale = std::max(middle - kVerticalMargin, 1.0);
  return middle - (static_cast<double>(value) * scale);
}

void WaveformView::PaintFrames(QPainter& painter) const {
  if (frames_.empty()) {
    return;
  }
  // The frames whose stretches are on screen.
  const auto count = static_cast<int>(frames_.size());
  const double first_sample = axis_.XToSample(0.0) + source_offset_;
  const double last_sample = axis_.XToSample(width()) + source_offset_;
  int first = 0;
  while (first < count &&
         static_cast<double>(FrameStretchStart(first + 1)) <= first_sample) {
    ++first;
  }
  for (int j = first;
       j < count && static_cast<double>(FrameStretchStart(j)) < last_sample;
       ++j) {
    const double left = XOfSample(static_cast<double>(FrameStretchStart(j)));
    const double right =
        XOfSample(static_cast<double>(FrameStretchStart(j + 1)));
    QColor fill;
    switch (lpc::FrameKind(frames_[static_cast<std::size_t>(j)])) {
      case lpc::FrameType::kVoiced:
        fill = colours_.voiced;
        break;
      case lpc::FrameType::kUnvoiced:
        fill = colours_.unvoiced;
        break;
      case lpc::FrameType::kRepeat:
        fill = colours_.repeat;
        break;
      case lpc::FrameType::kSilence:
      case lpc::FrameType::kStop:
        fill = colours_.silence;
        break;
    }
    painter.fillRect(QRectF(left, 0.0, right - left, height()), fill);
    if (frame_grid_) {
      painter.setPen(QPen(colours_.frame_grid, 0));
      painter.drawLine(QLineF(left, 0.0, left, height()));
    }
  }
}

void WaveformView::PaintWave(QPainter& painter) const {
  painter.setPen(QPen(colours_.wave, 0));
  const std::span<const float> samples = peaks_.Samples();
  if (axis_.samples_per_pixel < kJoinedSamplesPerPixel) {
    // Zoomed in: the samples joined, one point each, from the one before
    // the left edge to the one after the right.
    const auto first = static_cast<std::int64_t>(
        std::floor(axis_.XToSample(0.0) + source_offset_));
    const auto last = static_cast<std::int64_t>(
        std::ceil(axis_.XToSample(width()) + source_offset_));
    QPolygonF line;
    for (std::int64_t i = std::max<std::int64_t>(first, 0);
         i <= std::min<std::int64_t>(
                  last, static_cast<std::int64_t>(samples.size()) - 1);
         ++i) {
      line << QPointF(XOfSample(static_cast<double>(i)),
                      SampleY(samples[static_cast<std::size_t>(i)]));
    }
    painter.drawPolyline(line);
    return;
  }
  // One line per pixel, from the lowest to the highest sample under it
  // and the last sample of the pixel before, so neighbouring lines meet.
  for (int x = 0; x < width(); ++x) {
    const auto first = static_cast<std::int64_t>(
        std::floor(axis_.XToSample(x) + source_offset_));
    const auto last = static_cast<std::int64_t>(
        std::floor(axis_.XToSample(x + 1) + source_offset_));
    const std::int64_t end = std::max(last, first + 1);
    if (end <= 0 || std::cmp_greater_equal(first, samples.size())) {
      continue;
    }
    const Peak peak = peaks_.Range(first - 1, end);
    painter.drawLine(
        QLineF(x + 0.5, SampleY(peak.high), x + 0.5, SampleY(peak.low)));
  }
}

void WaveformView::PaintMarks(QPainter& painter) const {
  if (marker_.has_value()) {
    const double x = axis_.SampleToX(*marker_);
    painter.setPen(QPen(colours_.marker, 0));
    painter.drawLine(QLineF(x, 0.0, x, height()));
    QPolygonF flag;
    flag << QPointF(x, 0.0) << QPointF(x + kMarkerFlag, 0.0)
         << QPointF(x, kMarkerFlag);
    painter.setBrush(colours_.marker);
    painter.drawPolygon(flag);
    painter.setBrush(Qt::NoBrush);
  }
  if (playhead_.has_value()) {
    const double x = axis_.SampleToX(*playhead_);
    painter.setPen(QPen(colours_.playhead, 2));
    painter.drawLine(QLineF(x, 0.0, x, height()));
  }
}

}  // namespace speak5220::app
