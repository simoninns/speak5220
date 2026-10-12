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
// The basic options come first. Below them a button shows or hides the
// advanced ones (EncoderOptionInfo::advanced), and says how many of those
// differ from the baseline, so a hidden change is never missed; hidden
// options still apply. The form starts with them hidden.
//
// Editors by type: a slider (OptionSlider) over the option's typical range
// and step for real and whole numbers, with the value and unit beside it
// and a tick at the baseline's value; for an option that can be off (the
// squelch, the resonance hold), a check box that turns it on and a slider;
// for the pre-emphasis, a choice of chirp or first order, and a slider for the
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

  // The editor of `field`: its slider, or for the pre-emphasis its kind
  // chooser.
  QWidget* Editor(std::string_view field) const;
  // The slider of `field` (the coefficient's for the pre-emphasis).
  OptionSlider* Slider(std::string_view field) const;
  // The check box that turns an optional option (squelch_db,
  // resonance_hold_db) on; nullptr for other options.
  QCheckBox* OnBox(std::string_view field) const;
  QToolButton* ResetButton(std::string_view field) const;
  // Whether the row of `field` differs from the baseline.
  bool IsMarked(std::string_view field) const;

  // Enables or disables the editors and reset buttons; the advanced button
  // stays usable either way.
  void SetEditable(bool editable);
  bool IsEditable() const { return editable_; }

  // Shows or hides the advanced options, without signalling
  // AdvancedShownChanged().
  void SetAdvancedShown(bool shown);
  bool AdvancedShown() const;
  // The button that shows and hides them.
  QToolButton* AdvancedButton() const { return advanced_button_; }

 signals:
  // The user changed an option.
  void Edited();
  // The user showed or hid the advanced options.
  void AdvancedShownChanged(bool shown);

 private:
  struct Row {
    const core::encoder::EncoderOptionInfo* info = nullptr;
    QLabel* label = nullptr;
    // The editor's cell: the slider, or the box holding a check box or
    // chooser and a slider.
    QWidget* editor = nullptr;
    OptionSlider* slider = nullptr;
    // The check box that turns an optional option on.
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
  // Shows the advanced rows if the button is down, and names how many of
  // them are marked on the button.
  void UpdateAdvanced();

  std::vector<Row> rows_;
  QToolButton* advanced_button_;
  core::encoder::EncoderOptions options_;
  core::encoder::EncoderOptions baseline_;
  QString baseline_name_;
  bool loading_ = false;
  bool editable_ = true;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_ENCODER_SETTINGS_FORM_H_
