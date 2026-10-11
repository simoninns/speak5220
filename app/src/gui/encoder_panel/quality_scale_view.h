/*
 * File:        quality_scale_view.h
 * Module:      encoder_panel
 * Purpose:     Shows a PESQ score on its phrase's quality scale, from Bad
 *              to As good as LPC or better, with its band in words
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ENCODER_PANEL_QUALITY_SCALE_VIEW_H_
#define SPEAK5220_GUI_ENCODER_PANEL_QUALITY_SCALE_VIEW_H_

#include <QSize>
#include <QString>
#include <QWidget>
#include <optional>
#include <string_view>

#include "core/quality/quality_scale.h"
#include "gui/theme/colours.h"

class QEvent;
class QLabel;
class QPaintEvent;

namespace speak5220::app {

// The bar of the quality scale: the five bands from Bad to As good as LPC
// or better, each named inside it where it fits (the top band, from the
// anchor to the end, as "Best"), the score's mark, and a dashed mark for
// an earlier score (Optimise's start). At most 520 pixels wide, so it
// reads at a glance.
// GUI thread only.
class QualityBar : public QWidget {
  Q_OBJECT

 public:
  explicit QualityBar(QWidget* parent = nullptr);

  // Positions on the scale, 0-1 (QualityScale::Place()), or none.
  void SetPositions(std::optional<double> position,
                    std::optional<double> before);
  std::optional<double> Position() const { return position_; }
  std::optional<double> Before() const { return before_; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 protected:
  void changeEvent(QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  std::optional<double> position_;
  std::optional<double> before_;
  ThemeColours colours_;
};

// The band's name as the bar shows it: QualityScale::Label(), but "Best"
// for kLpcOrBetter. Pure.
std::string_view BandName(core::quality::QualityBand band);

// Plan §3.3, "Presenting PESQ to the user": the scale's bar, and under it
// the score, the phrase's anchor and the band in words (QualityText()), so
// it reads the same without colour. The tooltip explains PESQ
// (QualityToolTip()). GUI thread only.
class QualityScaleView : public QWidget {
  Q_OBJECT

 public:
  explicit QualityScaleView(QWidget* parent = nullptr);

  // Shows `score` against the phrase's `anchor`, with a mark for
  // `before` (an earlier score) if given. With no score the bar is empty
  // and the text says why (`placeholder`, or "Quality not measured").
  void SetScores(std::optional<double> score, std::optional<double> anchor,
                 std::optional<double> before = std::nullopt);
  void SetPlaceholder(const QString& placeholder);

  // The text under the bar.
  QString Text() const;
  // The score's placement, when it has a score and an anchor.
  std::optional<core::quality::QualityPlacement> Placement() const {
    return placement_;
  }
  QualityBar* Bar() const { return bar_; }

 private:
  QualityBar* bar_;
  QLabel* text_;
  std::optional<core::quality::QualityPlacement> placement_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_QUALITY_SCALE_VIEW_H_
