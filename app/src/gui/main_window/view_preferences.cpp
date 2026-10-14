/*
 * File:        view_preferences.cpp
 * Module:      main_window
 * Purpose:     What the user last chose to see in the window, kept between
 *              sessions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/view_preferences.h"

#include <QSettings>
#include <QString>
#include <QVariant>
#include <optional>
#include <string>
#include <string_view>

#include "core/dsp/output_filter.h"
#include "core/synth/chip_clock.h"
#include "core/synth/output_pin.h"
#include "gui/theme/colour_scheme.h"
#include "gui/waveform/phrase_tab.h"

namespace speak5220::app {

bool SettingsViewPreferences::AdvancedSettingsShown() {
  return settings_.value(QString::fromLatin1(kAdvancedSettingsKey), false)
      .toBool();
}

void SettingsViewPreferences::SetAdvancedSettingsShown(bool shown) {
  settings_.setValue(QString::fromLatin1(kAdvancedSettingsKey), shown);
}

PhraseTab SettingsViewPreferences::ChosenPhraseTab() {
  const std::string key = settings_.value(QString::fromLatin1(kPhraseTabKey))
                              .toString()
                              .toStdString();
  return PhraseTabFromKey(key).value_or(PhraseTab::kOriginal);
}

void SettingsViewPreferences::SetPhraseTab(PhraseTab tab) {
  const std::string_view key = PhraseTabKey(tab);
  settings_.setValue(
      QString::fromLatin1(kPhraseTabKey),
      QString::fromLatin1(key.data(), static_cast<qsizetype>(key.size())));
}

core::dsp::OutputFilter SettingsViewPreferences::ChosenOutputFilter() {
  const std::string key = settings_.value(QString::fromLatin1(kOutputFilterKey))
                              .toString()
                              .toStdString();
  return core::dsp::OutputFilterFromKey(key).value_or(
      core::dsp::OutputFilter::kNone);
}

void SettingsViewPreferences::SetOutputFilter(core::dsp::OutputFilter filter) {
  const std::string_view key = core::dsp::InfoFor(filter).key;
  settings_.setValue(
      QString::fromLatin1(kOutputFilterKey),
      QString::fromLatin1(key.data(), static_cast<qsizetype>(key.size())));
}

core::synth::OutputPin SettingsViewPreferences::ChosenOutputPin() {
  const std::string key = settings_.value(QString::fromLatin1(kOutputPinKey))
                              .toString()
                              .toStdString();
  return core::synth::OutputPinFromKey(key).value_or(
      core::synth::OutputPin::kSpeaker);
}

void SettingsViewPreferences::SetOutputPin(core::synth::OutputPin pin) {
  const std::string_view key = core::synth::InfoFor(pin).key;
  settings_.setValue(
      QString::fromLatin1(kOutputPinKey),
      QString::fromLatin1(key.data(), static_cast<qsizetype>(key.size())));
}

int SettingsViewPreferences::ChosenChipClock() {
  bool ok = false;
  const int hz = settings_.value(QString::fromLatin1(kChipClockKey)).toInt(&ok);
  return (ok ? core::synth::ChipClockFromHz(hz) : std::nullopt)
      .value_or(core::synth::kNominalClockHz);
}

void SettingsViewPreferences::SetChipClock(int clock_hz) {
  settings_.setValue(QString::fromLatin1(kChipClockKey), clock_hz);
}

ThemeChoice SettingsViewPreferences::ChosenTheme() {
  const std::string key =
      settings_.value(QString::fromLatin1(kThemeKey)).toString().toStdString();
  return ThemeChoiceFromKey(key).value_or(ThemeChoice::kAuto);
}

void SettingsViewPreferences::SetTheme(ThemeChoice choice) {
  const std::string_view key = ThemeChoiceKey(choice);
  settings_.setValue(
      QString::fromLatin1(kThemeKey),
      QString::fromLatin1(key.data(), static_cast<qsizetype>(key.size())));
}

}  // namespace speak5220::app
