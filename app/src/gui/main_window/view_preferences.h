/*
 * File:        view_preferences.h
 * Module:      main_window
 * Purpose:     What the user last chose to see in the window, kept between
 *              sessions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_MAIN_WINDOW_VIEW_PREFERENCES_H_
#define SPEAK5220_GUI_MAIN_WINDOW_VIEW_PREFERENCES_H_

#include <QSettings>

#include "core/dsp/output_filter.h"
#include "core/synth/output_pin.h"
#include "gui/theme/colour_scheme.h"
#include "gui/waveform/phrase_tab.h"

namespace speak5220::app {

// The user's choices of what the window shows, which belong to the user
// rather than to a project. Used on the GUI thread only.
class IViewPreferences {
 public:
  virtual ~IViewPreferences() = default;

  // Whether the encoder panel shows its advanced settings; false if never
  // saved.
  virtual bool AdvancedSettingsShown() = 0;
  virtual void SetAdvancedSettingsShown(bool shown) = 0;

  // The phrase editor's tab last chosen; Original if never saved, or
  // saved with a key this version does not know.
  virtual PhraseTab ChosenPhraseTab() = 0;
  virtual void SetPhraseTab(PhraseTab tab) = 0;

  // The audio stage the chip's output is heard and shown through; none if
  // never saved, or saved by a version that had a filter this one lacks.
  virtual core::dsp::OutputFilter ChosenOutputFilter() = 0;
  virtual void SetOutputFilter(core::dsp::OutputFilter filter) = 0;

  // The chip's pin the encoded audio is taken from; SPEAKER if never
  // saved, or saved with a key this version does not know.
  virtual core::synth::OutputPin ChosenOutputPin() = 0;
  virtual void SetOutputPin(core::synth::OutputPin pin) = 0;

  // The theme last chosen (View > Theme); Auto if never saved, or saved
  // with a key this version does not know.
  virtual ThemeChoice ChosenTheme() = 0;
  virtual void SetTheme(ThemeChoice choice) = 0;
};

// IViewPreferences in the application's QSettings (the platform's usual
// place for preferences).
class SettingsViewPreferences : public IViewPreferences {
 public:
  static constexpr const char* kAdvancedSettingsKey = "advancedEncoderSettings";
  // Holds the tab's key (PhraseTabKey()).
  static constexpr const char* kPhraseTabKey = "phraseTab";
  // Holds the filter's key (core::dsp::OutputFilterInfo::key).
  static constexpr const char* kOutputFilterKey = "outputFilter";
  // Holds the pin's key (core::synth::OutputPinInfo::key).
  static constexpr const char* kOutputPinKey = "outputPin";
  // Holds the choice's key (ThemeChoiceKey()).
  static constexpr const char* kThemeKey = "theme";

  bool AdvancedSettingsShown() override;
  void SetAdvancedSettingsShown(bool shown) override;
  PhraseTab ChosenPhraseTab() override;
  void SetPhraseTab(PhraseTab tab) override;
  core::dsp::OutputFilter ChosenOutputFilter() override;
  void SetOutputFilter(core::dsp::OutputFilter filter) override;
  core::synth::OutputPin ChosenOutputPin() override;
  void SetOutputPin(core::synth::OutputPin pin) override;
  ThemeChoice ChosenTheme() override;
  void SetTheme(ThemeChoice choice) override;

 private:
  QSettings settings_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_VIEW_PREFERENCES_H_
