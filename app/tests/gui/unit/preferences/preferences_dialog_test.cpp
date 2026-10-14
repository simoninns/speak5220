/*
 * File:        preferences_dialog_test.cpp
 * Module:      preferences tests
 * Purpose:     Unit tests of the Preferences dialog and of storing its
 *              choices, with a fake store and no audio devices
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/preferences/preferences_dialog.h"

#include <QByteArray>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QString>
#include <QTabWidget>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/util/logging.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/playback/audio_output.h"
#include "gui/support/gui_mocks.h"
#include "gui/theme/colour_scheme.h"
#include "gui/theme/colours.h"

namespace speak5220::app {
namespace {

using testing::FakeViewPreferences;

std::vector<AudioDeviceInfo> TwoDevices() {
  return {
      {.id = QByteArray("speakers"), .description = QStringLiteral("Speakers")},
      {.id = QByteArray("usb"), .description = QStringLiteral("USB Headset")}};
}

// Preferences that differ from the defaults in every field.
Preferences Chosen() {
  Preferences preferences;
  preferences.theme = ThemeChoice::kDark;
  preferences.waveform_colours = WaveformColours::kTheme;
  preferences.audio_device = QByteArray("usb");
  preferences.default_encoder.target_rms = 24.0;
  preferences.log_level = core::LogLevel::kDebug;
  preferences.log_file = QStringLiteral("/tmp/speak5220.log");
  return preferences;
}

TEST(PreferencesDialogTest, ShowsTheCurrentPreferences) {
  const PreferencesDialog dialog(Chosen(), TwoDevices());
  EXPECT_EQ(dialog.Tabs()->count(), 4);
  EXPECT_EQ(dialog.ThemeBox()->currentText(), QStringLiteral("Dark"));
  EXPECT_EQ(dialog.DeviceBox()->currentText(), QStringLiteral("USB Headset"));
  EXPECT_EQ(dialog.EncoderForm()->Options().target_rms, 24.0);
  EXPECT_TRUE(dialog.EncoderForm()->IsMarked("target_rms"));
  EXPECT_EQ(dialog.LogFileField()->text(),
            QStringLiteral("/tmp/speak5220.log"));
  EXPECT_EQ(dialog.Chosen(), Chosen());
}

TEST(PreferencesDialogTest, ListsTheSystemDefaultFirst) {
  const PreferencesDialog dialog(Preferences{}, TwoDevices());
  ASSERT_EQ(dialog.DeviceBox()->count(), 3);
  EXPECT_EQ(dialog.DeviceBox()->itemText(0), QStringLiteral("System default"));
  EXPECT_EQ(dialog.DeviceBox()->currentIndex(), 0);
  EXPECT_TRUE(dialog.Chosen().audio_device.isEmpty());
}

TEST(PreferencesDialogTest, KeepsADeviceThatIsNotConnected) {
  Preferences preferences;
  preferences.audio_device = QByteArray("bluetooth");
  const PreferencesDialog dialog(preferences, TwoDevices());
  EXPECT_EQ(dialog.DeviceBox()->count(), 4);
  EXPECT_EQ(dialog.DeviceBox()->currentText(),
            QStringLiteral("bluetooth (not connected)"));
  EXPECT_EQ(dialog.Chosen().audio_device, QByteArray("bluetooth"));
}

TEST(PreferencesDialogTest, GivesWhatTheUserChose) {
  PreferencesDialog dialog(Preferences{}, TwoDevices());
  dialog.ThemeBox()->setCurrentIndex(
      dialog.ThemeBox()->findData(static_cast<int>(ThemeChoice::kLight)));
  dialog.WaveformBox()->setCurrentIndex(dialog.WaveformBox()->findData(
      static_cast<int>(WaveformColours::kTheme)));
  dialog.DeviceBox()->setCurrentIndex(2);
  dialog.LogLevelBox()->setCurrentIndex(
      dialog.LogLevelBox()->findData(static_cast<int>(core::LogLevel::kTrace)));
  dialog.LogFileField()->setText(QStringLiteral("  run.log  "));
  const Preferences chosen = dialog.Chosen();
  EXPECT_EQ(chosen.theme, ThemeChoice::kLight);
  EXPECT_EQ(chosen.waveform_colours, WaveformColours::kTheme);
  EXPECT_EQ(chosen.audio_device, QByteArray("usb"));
  EXPECT_EQ(chosen.log_level, core::LogLevel::kTrace);
  EXPECT_EQ(chosen.log_file, QStringLiteral("run.log"));
}

TEST(PreferencesDialogTest, RestoreDefaultsResetsEveryTab) {
  PreferencesDialog dialog(Chosen(), TwoDevices());
  dialog.RestoreDefaultsButton()->click();
  EXPECT_EQ(dialog.Chosen(), Preferences{});
  EXPECT_FALSE(dialog.EncoderForm()->IsMarked("target_rms"));
}

TEST(PreferencesDialogTest, EveryChoiceHasAnItem) {
  const PreferencesDialog dialog(Preferences{}, {});
  EXPECT_EQ(dialog.ThemeBox()->count(), static_cast<int>(kThemeChoices.size()));
  EXPECT_EQ(dialog.WaveformBox()->count(),
            static_cast<int>(kWaveformColourChoices.size()));
  EXPECT_EQ(dialog.LogLevelBox()->count(), 3);
  EXPECT_EQ(dialog.DeviceBox()->count(), 1);
}

TEST(PreferencesTest, WriteThenReadGivesThemBack) {
  FakeViewPreferences store;
  EXPECT_EQ(ReadPreferences(store), Preferences{});
  WritePreferences(Chosen(), store);
  EXPECT_EQ(store.theme, ThemeChoice::kDark);
  EXPECT_EQ(store.audio_device, QByteArray("usb"));
  EXPECT_EQ(ReadPreferences(store), Chosen());
}

TEST(PreferencesTest, LogSettingsComeFromThePreferences) {
  FakeViewPreferences store;
  store.log_level = core::LogLevel::kTrace;
  store.log_file = QStringLiteral("saved.log");
  const core::LogSettings settings = LogSettingsFrom(store);
  EXPECT_EQ(settings.level, core::LogLevel::kTrace);
  EXPECT_TRUE(settings.console);
  EXPECT_EQ(settings.file_path, "saved.log");
}

}  // namespace
}  // namespace speak5220::app
