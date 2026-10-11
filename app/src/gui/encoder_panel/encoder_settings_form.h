/*
 * File:        encoder_settings_form.h
 * Module:      encoder_panel
 * Purpose:     The encoder's settings as a form, one editor per option,
 *              generated from the options' metadata
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ENCODER_PANEL_ENCODER_SETTINGS_FORM_H_
#define SPEAK5220_GUI_ENCODER_PANEL_ENCODER_SETTINGS_FORM_H_

#include <QString>
#include <QWidget>
#include <string_view>
#include <vector>

#include "core/encoder/encoder_options.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QToolButton;

namespace speak5220::app {

class OptionSlider;

// Plan §5: one row per EncoderOptionInfos() entry, in two columns: its
// label (the help text and default as the tooltip), an editor, and a
// button that puts it back to the baseline (the project's defaults for a
// phrase, the encoder's for the project). Rows that differ from the
// baseline have a bold label and the button enabled.
//
// Editors by type: a slider (OptionSlider) over the option's typical range
// and step for real and whole numbers, with the value and unit beside it
// and a tick at the baseline's value; a check box for on/off; for the
// target peak, a check box that turns it on and a slider; for the
// pre-emphasis, a choice of chirp or first order, and a slider for the
// coefficient. Values the sliders cannot show exactly (off their steps)
// are kept until that option is edited. GUI thread only.
class EncoderSettingsForm : public QWidget {
  Q_OBJECT

 public:
  explicit EncoderSettingsForm(QWidget* parent = nullptr);

  // Shows `options`, marking the rows that differ from `baseline`, without
  // signalling Edited(). `baseline_name` names the baseline in the reset
  // buttons' tooltips ("the project default").
  void SetOptions(const core::encoder::EncoderOptions& options,
                  const core::encoder::EncoderOptions& baseline,
                  const QString& baseline_name);
  // The options shown.
  const core::encoder::EncoderOptions& Options() const { return options_; }

  // The editor of `field`: its slider, its check box, or for the
  // pre-emphasis its kind chooser.
  QWidget* Editor(std::string_view field) const;
  // The slider of `field` (the coefficient's for the pre-emphasis), or
  // nullptr for an on/off option.
  OptionSlider* Slider(std::string_view field) const;
  // The check box that turns the target peak on.
  QCheckBox* PeakBox() const;
  QToolButton* ResetButton(std::string_view field) const;
  // Whether the row of `field` differs from the baseline.
  bool IsMarked(std::string_view field) const;

 signals:
  // The user changed an option.
  void Edited();

 private:
  struct Row {
    const core::encoder::EncoderOptionInfo* info = nullptr;
    QLabel* label = nullptr;
    OptionSlider* slider = nullptr;
    // The option's on/off check box, or the target peak's.
    QCheckBox* check = nullptr;
    QComboBox* kind = nullptr;
    QToolButton* reset = nullptr;
  };

  const Row* FindRow(std::string_view field) const;
  // Puts the value of options_ into the row's editors, and the baseline's
  // as the slider's tick.
  void ShowValue(const Row& row);
  // Reads the row's editors into options_ and signals Edited().
  void OnEdited(const Row& row);
  void UpdateMarks();

  std::vector<Row> rows_;
  core::encoder::EncoderOptions options_;
  core::encoder::EncoderOptions baseline_;
  QString baseline_name_;
  bool loading_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_ENCODER_SETTINGS_FORM_H_
