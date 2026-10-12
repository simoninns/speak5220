/*
 * File:        quality_rating_view.cpp
 * Module:      encoder_panel
 * Purpose:     Shows an encoding's quality: its rating from Poor to Good
 *              with the overall score, its three components and the
 *              figures behind them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/quality_rating_view.h"

#include <QEvent>
#include <QFontMetrics>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPaintEvent>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QProgressBar>
#include <QRectF>
#include <QSize>
#include <QString>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <optional>
#include <string_view>

#include "core/quality/quality_assessor.h"
#include "core/quality/quality_score.h"
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
// The component bars: their height and widest width.
constexpr int kComponentHeight = 14;
constexpr int kComponentWidth = 200;
// A component bar's steps: hundredths of its score.
constexpr int kComponentSteps = 100;
// The mark's triangle above the bar.
constexpr qreal kMarkHalfWidth = 5.0;
constexpr qreal kMarkHeight = 6.0;
constexpr qreal kMarkLine = 2.0;
// Space for a band's name inside it.
constexpr int kLabelPadding = 4;

// Each band's lowest score, worst first; the last runs to 1.
constexpr std::array<double, kQualityBandCount> kBandStarts = {
    0.0, quality::ScoreCalibration::kFairFrom,
    quality::ScoreCalibration::kGoodFrom};
constexpr std::array<quality::QualityRating, kQualityBandCount> kBands = {
    quality::QualityRating::kPoor, quality::QualityRating::kFair,
    quality::QualityRating::kGood};

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

RatingBar::RatingBar(QWidget* parent)
    : QWidget(parent), colours_(ColoursFor(palette())) {
  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
  setMaximumWidth(kMaximumWidth);
}

void RatingBar::SetScores(std::optional<double> score,
                          std::optional<double> before) {
  score_ = score;
  before_ = before;
  update();
}

QSize RatingBar::sizeHint() const {
  return {kPreferredWidth, kBarHeight + static_cast<int>(kMarkHeight)};
}

QSize RatingBar::minimumSizeHint() const {
  return {kMinimumWidth, kBarHeight + static_cast<int>(kMarkHeight)};
}

void RatingBar::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange) {
    colours_ = ColoursFor(palette());
    update();
  }
  QWidget::changeEvent(event);
}

void RatingBar::paintEvent(QPaintEvent* /*event*/) {
  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing);
  const QRectF bar(0.5, kMarkHeight + 0.5, width() - 1.0, kBarHeight - 1.0);
  const QFontMetrics metrics = fontMetrics();

  for (std::size_t band = 0; band < kQualityBandCount; ++band) {
    const double begin = kBandStarts.at(band);
    const double end =
        band + 1 == kQualityBandCount ? 1.0 : kBandStarts.at(band + 1);
    const QRectF rect(bar.left() + (begin * bar.width()), bar.top(),
                      (end - begin) * bar.width(), bar.height());
    painter.fillRect(rect, colours_.quality_bands.at(band));
    painter.setPen(QPen(palette().color(QPalette::Mid), 1.0));
    painter.drawRect(rect);
    // Its name, if it fits, so the bands read without colour.
    const QString name = ToQString(quality::RatingLabel(kBands.at(band)));
    if (metrics.horizontalAdvance(name) + (2 * kLabelPadding) <= rect.width()) {
      painter.setPen(colours_.quality_mark);
      painter.drawText(rect, Qt::AlignCenter, name);
    }
  }

  auto x_of = [&bar](double score) {
    return bar.left() + (std::clamp(score, 0.0, 1.0) * bar.width());
  };
  if (before_.has_value()) {
    painter.setPen(QPen(colours_.quality_mark, kMarkLine, Qt::DashLine));
    const qreal x = x_of(*before_);
    painter.drawLine(QPointF(x, bar.top()), QPointF(x, bar.bottom()));
  }
  if (score_.has_value()) {
    const qreal x = x_of(*score_);
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

QualityRatingView::QualityRatingView(QWidget* parent)
    : QWidget(parent),
      bar_(new RatingBar(this)),
      text_(new QLabel(this)),
      figures_(new QLabel(this)) {
  const std::array<QString, kComponentCount> names = {
      QStringLiteral("Envelope"), QStringLiteral("Pitch"),
      QStringLiteral("Intelligibility")};
  auto* components = new QGridLayout;
  components->setContentsMargins(0, 0, 0, 0);
  for (int c = 0; c < kComponentCount; ++c) {
    const auto i = static_cast<std::size_t>(c);
    names_.at(i) = new QLabel(names.at(i), this);
    auto* component = new QProgressBar(this);
    component->setRange(0, kComponentSteps);
    component->setTextVisible(true);
    component->setFixedHeight(kComponentHeight);
    component->setMaximumWidth(kComponentWidth);
    component->setAccessibleName(names.at(i));
    components_.at(i) = component;
    components->addWidget(names_.at(i), c, 0);
    components->addWidget(component, c, 1);
  }
  components->setColumnStretch(2, 1);
  figures_->setTextInteractionFlags(Qt::TextSelectableByMouse);
  figures_->setWordWrap(true);

  auto* row = new QHBoxLayout;
  row->addWidget(bar_);
  row->addStretch();
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addLayout(row);
  layout->addWidget(text_);
  layout->addLayout(components);
  layout->addWidget(figures_);
  setToolTip(QualityToolTip());
  setAccessibleName(QStringLiteral("Quality"));
  SetAssessment(std::nullopt);
}

void QualityRatingView::SetAssessment(
    const std::optional<quality::QualityAssessment>& assessment,
    std::optional<double> before) {
  if (!assessment.has_value()) {
    SetPlaceholder(QualityText(std::nullopt));
    return;
  }
  const quality::QualityScore& score = assessment->score;
  bar_->SetScores(score.overall, before);
  QString text = QualityText(score.overall);
  if (before.has_value()) {
    text += QStringLiteral(" · from %1").arg(*before, 0, 'f', 2);
  }
  text_->setText(text);
  text_->setAccessibleName(text);
  const std::array<std::optional<double>, kComponentCount> values = {
      score.components.envelope, score.components.pitch,
      score.components.intelligibility};
  for (std::size_t c = 0; c < values.size(); ++c) {
    QProgressBar* component = components_.at(c);
    if (const std::optional<double>& value = values.at(c); value.has_value()) {
      component->setValue(
          static_cast<int>(std::lround(*value * kComponentSteps)));
      component->setFormat(QString::number(*value, 'f', 2));
    } else {
      component->reset();
      component->setFormat(QStringLiteral("not measured"));
    }
  }
  figures_->setText(FiguresText(assessment->measures));
  ShowComponents(true);
}

void QualityRatingView::SetScore(double overall, std::optional<double> before) {
  bar_->SetScores(overall, before);
  QString text = QualityText(overall);
  if (before.has_value()) {
    text += QStringLiteral(" · from %1").arg(*before, 0, 'f', 2);
  }
  text_->setText(text);
  text_->setAccessibleName(text);
  ShowComponents(false);
}

void QualityRatingView::SetPlaceholder(const QString& placeholder) {
  bar_->SetScores(std::nullopt, std::nullopt);
  text_->setText(placeholder);
  text_->setAccessibleName(placeholder);
  for (QProgressBar* component : components_) {
    component->reset();
    component->setFormat(QString());
  }
  figures_->clear();
  ShowComponents(false);
}

QString QualityRatingView::Text() const { return text_->text(); }

QString QualityRatingView::Figures() const { return figures_->text(); }

QProgressBar* QualityRatingView::ComponentBar(Component component) const {
  return components_.at(static_cast<std::size_t>(component));
}

void QualityRatingView::ShowComponents(bool shown) {
  for (std::size_t c = 0; c < components_.size(); ++c) {
    names_.at(c)->setVisible(shown);
    components_.at(c)->setVisible(shown);
  }
  figures_->setVisible(shown);
}

}  // namespace speak5220::app
