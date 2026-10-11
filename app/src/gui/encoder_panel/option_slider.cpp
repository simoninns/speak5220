/*
 * File:        option_slider.cpp
 * Module:      encoder_panel
 * Purpose:     A slider for one encoder option: its value and unit beside
 *              it, and a tick where its default lies
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/option_slider.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPointF>
#include <QPolygonF>
#include <QRect>
#include <QSlider>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <optional>
#include <utility>

namespace speak5220::app {

namespace {

// The default's tick under the groove, in pixels.
constexpr qreal kTickHalfWidth = 5.0;
constexpr qreal kTickHeight = 6.0;
// Room under the groove for the tick.
constexpr int kExtraHeight = 6;
// Steps with up to this many decimals are rounded to them.
constexpr int kMaxDecimals = 6;
// Ten, the base of the decimals.
constexpr double kTen = 10.0;

// The decimals of `step` (0.25 has 2), so values on the steps round to
// short text.
int DecimalsOf(double step) {
  int decimals = 0;
  double scaled = step;
  while (decimals < kMaxDecimals &&
         std::abs(scaled - std::round(scaled)) > 1e-9) {
    scaled *= kTen;
    ++decimals;
  }
  return decimals;
}

}  // namespace

DefaultTickSlider::DefaultTickSlider(QWidget* parent)
    : QSlider(Qt::Horizontal, parent) {
  setMinimumHeight(sizeHint().height() + kExtraHeight);
}

void DefaultTickSlider::SetDefaultPosition(std::optional<int> position) {
  default_position_ = position;
  update();
}

void DefaultTickSlider::paintEvent(QPaintEvent* event) {
  QSlider::paintEvent(event);
  if (!default_position_.has_value() || *default_position_ < minimum() ||
      *default_position_ > maximum()) {
    return;
  }
  QStyleOptionSlider option;
  initStyleOption(&option);
  const QRect groove = style()->subControlRect(QStyle::CC_Slider, &option,
                                               QStyle::SC_SliderGroove, this);
  const QRect handle = style()->subControlRect(QStyle::CC_Slider, &option,
                                               QStyle::SC_SliderHandle, this);
  // The handle's centre runs from half a handle in from each end.
  const int span = groove.width() - handle.width();
  const int offset = QStyle::sliderPositionFromValue(
      minimum(), maximum(), *default_position_, span, option.upsideDown);
  const qreal x = groove.left() + (handle.width() / 2.0) + offset;
  const qreal top = handle.bottom() + 1.0;
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  painter.setPen(Qt::NoPen);
  painter.setBrush(palette().color(QPalette::WindowText));
  QPolygonF tick;
  tick << QPointF(x, top) << QPointF(x + kTickHalfWidth, top + kTickHeight)
       << QPointF(x - kTickHalfWidth, top + kTickHeight);
  painter.drawPolygon(tick);
}

OptionSlider::OptionSlider(double minimum, double maximum, double step,
                           Formatter format, QWidget* parent)
    : QWidget(parent),
      typical_minimum_(minimum),
      typical_maximum_(maximum),
      minimum_(minimum),
      maximum_(maximum),
      step_(step),
      decimals_(DecimalsOf(step)),
      format_(std::move(format)),
      slider_(new DefaultTickSlider(this)),
      text_(new QLabel(this)) {
  slider_->setSingleStep(1);
  slider_->setPageStep(std::max(
      1, static_cast<int>(std::lround((maximum - minimum) / step / kTen))));
  const QFontMetrics metrics = text_->fontMetrics();
  text_->setMinimumWidth(std::max(metrics.horizontalAdvance(format_(minimum)),
                                  metrics.horizontalAdvance(format_(maximum))));
  text_->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  auto* layout = new QHBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(slider_, 1);
  layout->addWidget(text_);
  connect(slider_, &QSlider::valueChanged, this, &OptionSlider::OnMoved);
  SetValue(minimum);
}

void OptionSlider::SetValue(double value) {
  loading_ = true;
  value_ = value;
  FitRange(value);
  slider_->setValue(PositionOf(value));
  text_->setText(format_(value));
  loading_ = false;
}

void OptionSlider::SetDefault(std::optional<double> value) {
  default_ = value;
  FitRange(value_);
}

double OptionSlider::ValueAt(int position) const {
  // The ends may be widened values off the steps.
  if (position <= 0) {
    return minimum_;
  }
  if (position >= slider_->maximum()) {
    return maximum_;
  }
  const double scale = std::pow(kTen, decimals_);
  return std::round((minimum_ + (position * step_)) * scale) / scale;
}

int OptionSlider::PositionOf(double value) const {
  return std::clamp(static_cast<int>(std::lround((value - minimum_) / step_)),
                    slider_->minimum(), slider_->maximum());
}

void OptionSlider::MoveTo(double value) {
  slider_->setValue(PositionOf(value));
}

QString OptionSlider::Text() const { return text_->text(); }

int OptionSlider::TextWidth() const { return text_->minimumWidth(); }

void OptionSlider::SetTextWidth(int width) { text_->setMinimumWidth(width); }

void OptionSlider::FitRange(double value) {
  const bool loading = loading_;
  loading_ = true;
  minimum_ = std::min(typical_minimum_, value);
  maximum_ = std::max(typical_maximum_, value);
  slider_->setRange(
      0, static_cast<int>(std::ceil(((maximum_ - minimum_) / step_) - 1e-9)));
  slider_->setValue(PositionOf(value_));
  slider_->SetDefaultPosition(default_.has_value()
                                  ? std::optional<int>(PositionOf(*default_))
                                  : std::nullopt);
  loading_ = loading;
}

void OptionSlider::OnMoved(int position) {
  if (loading_) {
    return;
  }
  value_ = ValueAt(position);
  text_->setText(format_(value_));
  emit ValueChanged(value_);
}

}  // namespace speak5220::app
