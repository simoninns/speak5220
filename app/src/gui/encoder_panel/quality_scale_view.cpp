/*
 * File:        quality_scale_view.cpp
 * Module:      encoder_panel
 * Purpose:     Shows a PESQ score on its phrase's quality scale, from Bad
 *              to As good as LPC or better, with its band in words
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/quality_scale_view.h"

#include <QEvent>
#include <QFontMetrics>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cstddef>
#include <optional>
#include <string_view>

#include "core/quality/quality_scale.h"
#include "gui/encoder_panel/encoding_summary.h"
#include "gui/theme/colours.h"

namespace speak5220::app {

namespace {

namespace quality = core::quality;

// The bar's size, in device-independent pixels.
constexpr int kBarHeight = 22;
// Short enough to take in at a glance, however wide the panel.
constexpr int kPreferredWidth = 480;
constexpr int kMaximumWidth = 520;
constexpr int kMinimumWidth = 160;
// The mark's triangle above the bar.
constexpr qreal kMarkHalfWidth = 5.0;
constexpr qreal kMarkHeight = 6.0;
constexpr qreal kMarkLine = 2.0;
// Space for a band's name inside it.
constexpr int kLabelPadding = 4;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

std::string_view BandName(quality::QualityBand band) {
  // The top band is too narrow for its full label, which the text under
  // the bar gives.
  return band == quality::QualityBand::kLpcOrBetter
             ? "Best"
             : quality::QualityScale::Label(band);
}

QualityBar::QualityBar(QWidget* parent)
    : QWidget(parent), colours_(ColoursFor(palette())) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMaximumWidth(kMaximumWidth);
}

void QualityBar::SetPositions(std::optional<double> position,
                              std::optional<double> before) {
  position_ = position;
  before_ = before;
  update();
}

QSize QualityBar::sizeHint() const {
  return {kPreferredWidth, kBarHeight + static_cast<int>(kMarkHeight)};
}

QSize QualityBar::minimumSizeHint() const {
  return {kMinimumWidth, kBarHeight + static_cast<int>(kMarkHeight)};
}

void QualityBar::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange) {
    colours_ = ColoursFor(palette());
    update();
  }
  QWidget::changeEvent(event);
}

void QualityBar::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF bar(0.5, kMarkHeight + 0.5, width() - 1.0, kBarHeight - 1.0);
  const QFontMetrics metrics = fontMetrics();

  // The bands, each from its start to the next one's; the top one runs
  // from the anchor to the end of the bar.
  using quality::QualityScale;
  for (std::size_t band = 0; band < kQualityBandCount; ++band) {
    const double begin =
        band == 0
            ? 0.0
            : QualityScale::PositionOf(QualityScale::kBandStarts.at(band - 1));
    const double end =
        band + 1 == kQualityBandCount
            ? 1.0
            : QualityScale::PositionOf(QualityScale::kBandStarts.at(band));
    const QRectF rect(bar.left() + (begin * bar.width()), bar.top(),
                      (end - begin) * bar.width(), bar.height());
    painter.fillRect(rect, colours_.quality_bands.at(band));
    painter.setPen(QPen(palette().color(QPalette::Mid), 1.0));
    painter.drawRect(rect);
    // Its name, if it fits, so the bands read without colour.
    const QString name =
        ToQString(BandName(static_cast<quality::QualityBand>(band)));
    if (metrics.horizontalAdvance(name) + (2 * kLabelPadding) <= rect.width()) {
      painter.setPen(colours_.quality_mark);
      painter.drawText(rect, Qt::AlignCenter, name);
    }
  }

  auto x_of = [&bar](double position) {
    return bar.left() + (std::clamp(position, 0.0, 1.0) * bar.width());
  };
  if (before_.has_value()) {
    QPen dashed(colours_.quality_mark, kMarkLine, Qt::DashLine);
    painter.setPen(dashed);
    const qreal x = x_of(*before_);
    painter.drawLine(QPointF(x, bar.top()), QPointF(x, bar.bottom()));
  }
  if (position_.has_value()) {
    const qreal x = x_of(*position_);
    painter.setPen(QPen(colours_.quality_mark, kMarkLine));
    painter.drawLine(QPointF(x, bar.top()), QPointF(x, bar.bottom()));
    QPolygonF triangle;
    triangle << QPointF(x - kMarkHalfWidth, 0.0)
             << QPointF(x + kMarkHalfWidth, 0.0) << QPointF(x, kMarkHeight);
    painter.setPen(Qt::NoPen);
    painter.setBrush(colours_.quality_mark);
    painter.drawPolygon(triangle);
  }
}

QualityScaleView::QualityScaleView(QWidget* parent)
    : QWidget(parent), bar_(new QualityBar(this)), text_(new QLabel(this)) {
  auto* row = new QHBoxLayout;
  row->addWidget(bar_);
  row->addStretch();
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(row);
  layout->addWidget(text_);
  setToolTip(QualityToolTip());
  setAccessibleName(QStringLiteral("Quality"));
  SetScores(std::nullopt, std::nullopt);
}

void QualityScaleView::SetScores(std::optional<double> score,
                                 std::optional<double> anchor,
                                 std::optional<double> before) {
  placement_.reset();
  std::optional<double> before_position;
  if (score.has_value() && anchor.has_value()) {
    placement_ = quality::QualityScale::Place(*score, *anchor);
    if (before.has_value()) {
      before_position = quality::QualityScale::Place(*before, *anchor).position;
    }
  }
  bar_->SetPositions(placement_.has_value()
                         ? std::optional<double>(placement_->position)
                         : std::nullopt,
                     before_position);
  QString text = QualityText(score, anchor);
  if (before.has_value() && score.has_value()) {
    text += QStringLiteral(" · from %1").arg(*before, 0, 'f', 2);
  }
  text_->setText(text);
  text_->setAccessibleName(text);
}

void QualityScaleView::SetPlaceholder(const QString& placeholder) {
  placement_.reset();
  bar_->SetPositions(std::nullopt, std::nullopt);
  text_->setText(placeholder);
}

QString QualityScaleView::Text() const { return text_->text(); }

}  // namespace speak5220::app
