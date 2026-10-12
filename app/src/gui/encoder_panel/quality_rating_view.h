/*
 * File:        quality_rating_view.h
 * Module:      encoder_panel
 * Purpose:     Shows an encoding's quality: its rating from Poor to Good
 *              with the overall score, its three components and the
 *              figures behind them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ENCODER_PANEL_QUALITY_RATING_VIEW_H_
#define SPEAK5220_GUI_ENCODER_PANEL_QUALITY_RATING_VIEW_H_

#include <QSize>
#include <QString>
#include <QWidget>
#include <array>
#include <optional>

#include "core/quality/quality_assessor.h"
#include "gui/theme/colours.h"

class QEvent;
class QLabel;
class QPaintEvent;
class QProgressBar;

namespace speak5220::app {

// The rating's bar: the bands Poor, Fair and Good over the overall score's
// 0 to 1, each named inside it, the score's mark, and a dashed mark for an
// earlier score (Optimise's start). At most 520 pixels wide, so it reads
// at a glance. GUI thread only.
class RatingBar : public QWidget {
  Q_OBJECT

 public:
  explicit RatingBar(QWidget* parent = nullptr);

  // Overall scores, 0 to 1, or none.
  void SetScores(std::optional<double> score, std::optional<double> before);
  std::optional<double> Score() const { return score_; }
  std::optional<double> Before() const { return before_; }

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 protected:
  void changeEvent(QEvent* event) override;
  void paintEvent(QPaintEvent* event) override;

 private:
  std::optional<double> score_;
  std::optional<double> before_;
  ThemeColours colours_;
};

// Plan §3.3, "Presenting quality to the user": the rating's bar, and under
// it the rating and score in words (QualityText()), so it reads the same
// without colour; a small bar for each component, with its score as text;
// and the figures behind them (FiguresText()). The tooltip explains the
// measures (QualityToolTip()). GUI thread only.
class QualityRatingView : public QWidget {
  Q_OBJECT

 public:
  // The components, in the order their bars are shown.
  enum class Component { kEnvelope, kPitch, kIntelligibility };
  static constexpr int kComponentCount = 3;

  explicit QualityRatingView(QWidget* parent = nullptr);

  // Shows `assessment`, with a mark for `before` (an earlier overall
  // score) if given. With none the bars are empty and the text says
  // "Quality not measured".
  void SetAssessment(
      const std::optional<core::quality::QualityAssessment>& assessment,
      std::optional<double> before = std::nullopt);
  // Shows an overall score alone (Optimise's best so far), with a mark for
  // `before`; the components and figures are hidden.
  void SetScore(double overall, std::optional<double> before = std::nullopt);
  // Empties the bars and shows `placeholder` instead.
  void SetPlaceholder(const QString& placeholder);

  // The text under the bar, and the figures' text.
  QString Text() const;
  QString Figures() const;
  RatingBar* Bar() const { return bar_; }
  // A component's bar; its text is the component's score, or empty.
  QProgressBar* ComponentBar(Component component) const;

 private:
  void ShowComponents(bool shown);

  RatingBar* bar_;
  QLabel* text_;
  std::array<QLabel*, kComponentCount> names_{};
  std::array<QProgressBar*, kComponentCount> components_{};
  QLabel* figures_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_QUALITY_RATING_VIEW_H_
