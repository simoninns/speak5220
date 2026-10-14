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

#include <QByteArray>
#include <QSettings>
#include <QString>

#include "core/dsp/output_filter.h"
#include "core/encoder/encoder_options.h"
#include "core/synth/chip_clock.h"
#include "core/synth/output_pin.h"
#include "core/util/logging.h"
#include "gui/theme/colour_scheme.h"
#include "gui/theme/colours.h"
#include "gui/waveform/phrase_tab.h"

namespace speak5220::app {

// The user's choices, which belong to the user rather than to a project:
// what the window shows (its menus' choices) and the Preferences dialog's
// settings (PreferencesDialog). Used on the GUI thread only.
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

  // The chip's clock its output is heard at, in Hz; the nominal clock if
  // never saved, or saved as a rate this version does not offer
  // (core::synth::ChipClocks()).
  virtual int ChosenChipClock() = 0;
  virtual void SetChipClock(int clock_hz) = 0;

  // The theme last chosen (View > Theme); Auto if never saved, or saved
  // with a key this version does not know.
  virtual ThemeChoice ChosenTheme() = 0;
  virtual void SetTheme(ThemeChoice choice) = 0;

  // How the waveforms are coloured; dark if never saved, or saved with a
  // key this version does not know.
  virtual WaveformColours ChosenWaveformColours() = 0;
  virtual void SetWaveformColours(WaveformColours choice) = 0;

  // The audio output device's id (AudioDeviceInfo::id); empty, the
  // system's default, if never saved.
  virtual QByteArray ChosenAudioDevice() = 0;
  virtual void SetAudioDevice(const QByteArray& id) = 0;

  // The settings new projects start from (ProjectSettings::default_encoder),
  // of both stages; the encoder's own defaults if never saved, and for any
  // option saved with a value this version does not accept.
  virtual core::encoder::EncoderOptions ChosenDefaultEncoder() = 0;
  virtual void SetDefaultEncoder(
      const core::encoder::EncoderOptions& options) = 0;

  // The log level and the log file of the next start (plan §2.6); info
  // and none if never saved. A command-line option overrides them.
  virtual core::LogLevel ChosenLogLevel() = 0;
  virtual void SetLogLevel(core::LogLevel level) = 0;
  // Empty for no log file.
  virtual QString ChosenLogFile() = 0;
  virtual void SetLogFile(const QString& path) = 0;
};

// The log settings the preferences give: the level and file chosen, with
// the console on.
core::LogSettings LogSettingsFrom(IViewPreferences& preferences);

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
  // Holds the clock in Hz.
  static constexpr const char* kChipClockKey = "chipClockHz";
  // Holds the choice's key (ThemeChoiceKey()).
  static constexpr const char* kThemeKey = "theme";
  // Holds the choice's key (WaveformColoursKey()).
  static constexpr const char* kWaveformColoursKey = "waveformColours";
  // Holds the device's id.
  static constexpr const char* kAudioDeviceKey = "audioDevice";
  // A group holding the options that differ from the encoder's defaults,
  // by field name (EncoderOptionInfo::field), as text
  // (EncoderOptionText()).
  static constexpr const char* kDefaultEncoderGroup = "defaultEncoder";
  // Hold the level's name (core::LogLevelName()) and the file's path.
  static constexpr const char* kLogLevelKey = "logLevel";
  static constexpr const char* kLogFileKey = "logFile";

  bool AdvancedSettingsShown() override;
  void SetAdvancedSettingsShown(bool shown) override;
  PhraseTab ChosenPhraseTab() override;
  void SetPhraseTab(PhraseTab tab) override;
  core::dsp::OutputFilter ChosenOutputFilter() override;
  void SetOutputFilter(core::dsp::OutputFilter filter) override;
  core::synth::OutputPin ChosenOutputPin() override;
  void SetOutputPin(core::synth::OutputPin pin) override;
  int ChosenChipClock() override;
  void SetChipClock(int clock_hz) override;
  ThemeChoice ChosenTheme() override;
  void SetTheme(ThemeChoice choice) override;
  WaveformColours ChosenWaveformColours() override;
  void SetWaveformColours(WaveformColours choice) override;
  QByteArray ChosenAudioDevice() override;
  void SetAudioDevice(const QByteArray& id) override;
  core::encoder::EncoderOptions ChosenDefaultEncoder() override;
  void SetDefaultEncoder(const core::encoder::EncoderOptions& options) override;
  core::LogLevel ChosenLogLevel() override;
  void SetLogLevel(core::LogLevel level) override;
  QString ChosenLogFile() override;
  void SetLogFile(const QString& path) override;

 private:
  QSettings settings_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_VIEW_PREFERENCES_H_
