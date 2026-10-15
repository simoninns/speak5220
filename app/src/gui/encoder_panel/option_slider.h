/*
 * File:        option_slider.h
 * Module:      encoder_panel
 * Purpose:     A slider for one encoder option: its value and unit beside
 *              it, and a tick where its default lies
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ENCODER_PANEL_OPTION_SLIDER_H_
#define SPEAK5220_GUI_ENCODER_PANEL_OPTION_SLIDER_H_

#include <QSlider>
#include <QString>
#include <QWidget>
#include <functional>
#include <optional>

class QLabel;
class QPaintEvent;

namespace speak5220::app {

// A horizontal QSlider that also marks one position, the default, with a
// tick under its groove. GUI thread only.
class DefaultTickSlider : public QSlider {
  Q_OBJECT

 public:
  explicit DefaultTickSlider(QWidget* parent = nullptr);

  // The position to mark, or none.
  void SetDefaultPosition(std::optional<int> position);
  std::optional<int> DefaultPosition() const { return default_position_; }

 protected:
  void paintEvent(QPaintEvent* event) override;

 private:
  std::optional<int> default_position_;
};

// One option's slider (the settings form): positions from a
// minimum to a maximum in equal steps, the value shown beside it as text
// (`format`), and a tick at the default. A value off the steps, or outside
// the range (from a project file or Optimise), is shown exactly and widens
// the range to include it; moving the slider gives values on the steps.
// GUI thread only.
class OptionSlider : public QWidget {
  Q_OBJECT

 public:
  using Formatter = std::function<QString(double)>;

  // `minimum` < `maximum`, `step` > 0; `format` gives a value's text with
  // its unit. The text's width is fixed by the widest of the range's ends.
  OptionSlider(double minimum, double maximum, double step, Formatter format,
               QWidget* parent = nullptr);

  // Shows `value` without signalling.
  void SetValue(double value);
  double Value() const { return value_; }
  // Marks `value` as the default, or nothing.
  void SetDefault(std::optional<double> value);

  // The value at slider position `position`, and the position nearest
  // `value`.
  double ValueAt(int position) const;
  int PositionOf(double value) const;
  // Moves the slider to the position nearest `value`, as a user would
  // (signals ValueChanged()).
  void MoveTo(double value);

  DefaultTickSlider* Slider() const { return slider_; }
  QString Text() const;
  // The width the value's text needs at the range's ends, and the width
  // to give it (so sliders in a column line up).
  int TextWidth() const;
  void SetTextWidth(int width);

 signals:
  // The user moved the slider.
  void ValueChanged(double value);

 private:
  // Sets the range to the typical one widened to include `value`.
  void FitRange(double value);
  void OnMoved(int position);

  double typical_minimum_;
  double typical_maximum_;
  // The range now: the typical one, widened.
  double minimum_;
  double maximum_;
  double step_;
  int decimals_;
  Formatter format_;
  DefaultTickSlider* slider_;
  QLabel* text_;
  double value_ = 0.0;
  std::optional<double> default_;
  bool loading_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_OPTION_SLIDER_H_
