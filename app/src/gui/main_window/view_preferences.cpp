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

#include <QByteArray>
#include <QSettings>
#include <QString>
#include <QVariant>
#include <optional>
#include <string>
#include <string_view>

#include "core/dsp/output_filter.h"
#include "core/encoder/encoder_options.h"
#include "core/project/phrase.h"
#include "core/synth/chip_clock.h"
#include "core/synth/output_pin.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "gui/theme/colour_scheme.h"
#include "gui/theme/colours.h"
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

WaveformColours SettingsViewPreferences::ChosenWaveformColours() {
  const std::string key =
      settings_.value(QString::fromLatin1(kWaveformColoursKey))
          .toString()
          .toStdString();
  return WaveformColoursFromKey(key).value_or(WaveformColours::kDark);
}

void SettingsViewPreferences::SetWaveformColours(WaveformColours choice) {
  const std::string_view key = WaveformColoursKey(choice);
  settings_.setValue(
      QString::fromLatin1(kWaveformColoursKey),
      QString::fromLatin1(key.data(), static_cast<qsizetype>(key.size())));
}

QByteArray SettingsViewPreferences::ChosenAudioDevice() {
  return settings_.value(QString::fromLatin1(kAudioDeviceKey)).toByteArray();
}

void SettingsViewPreferences::SetAudioDevice(const QByteArray& id) {
  settings_.setValue(QString::fromLatin1(kAudioDeviceKey), id);
}

core::encoder::EncoderOptions SettingsViewPreferences::ChosenDefaultEncoder() {
  core::encoder::EncoderOptions options;
  settings_.beginGroup(QString::fromLatin1(kDefaultEncoderGroup));
  for (const QString& field : settings_.childKeys()) {
    const std::string text = settings_.value(field).toString().toStdString();
    // A value another version saved, which this one does not accept,
    // leaves that option at its default.
    if (const core::Status status =
            core::encoder::SetEncoderOption(options, field.toStdString(), text);
        !status.IsOk()) {
      core::Logger(core::kLogModuleGui)
          ->warn("default encoder setting {} ignored: {}", field.toStdString(),
                 status.Message());
    }
  }
  settings_.endGroup();
  return options;
}

void SettingsViewPreferences::SetDefaultEncoder(
    const core::encoder::EncoderOptions& options) {
  settings_.beginGroup(QString::fromLatin1(kDefaultEncoderGroup));
  settings_.remove(QString());
  for (const auto& [field, text] :
       core::project::OverridesFor(core::encoder::EncoderOptions{}, options)) {
    settings_.setValue(QString::fromStdString(field),
                       QString::fromStdString(text));
  }
  settings_.endGroup();
}

core::LogLevel SettingsViewPreferences::ChosenLogLevel() {
  const core::StatusOr<core::LogLevel> level =
      core::ParseLogLevel(settings_.value(QString::fromLatin1(kLogLevelKey))
                              .toString()
                              .toStdString());
  return level.IsOk() ? *level : core::LogLevel::kInfo;
}

void SettingsViewPreferences::SetLogLevel(core::LogLevel level) {
  const std::string_view name = core::LogLevelName(level);
  settings_.setValue(
      QString::fromLatin1(kLogLevelKey),
      QString::fromLatin1(name.data(), static_cast<qsizetype>(name.size())));
}

QString SettingsViewPreferences::ChosenLogFile() {
  return settings_.value(QString::fromLatin1(kLogFileKey)).toString();
}

void SettingsViewPreferences::SetLogFile(const QString& path) {
  settings_.setValue(QString::fromLatin1(kLogFileKey), path);
}

core::LogSettings LogSettingsFrom(IViewPreferences& preferences) {
  return {.level = preferences.ChosenLogLevel(),
          .console = true,
          .file_path = preferences.ChosenLogFile().toStdString()};
}

}  // namespace speak5220::app
