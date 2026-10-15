/*
 * File:        preferences_dialog.h
 * Module:      preferences
 * Purpose:     The Preferences dialog: appearance, the audio device, the
 *              settings new projects start from, and logging
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PREFERENCES_PREFERENCES_DIALOG_H_
#define SPEAK5220_GUI_PREFERENCES_PREFERENCES_DIALOG_H_

#include <QByteArray>
#include <QDialog>
#include <QString>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/util/logging.h"
#include "gui/main_window/view_preferences.h"
#include "gui/playback/audio_output.h"
#include "gui/theme/colour_scheme.h"
#include "gui/theme/colours.h"

class QComboBox;
class QLineEdit;
class QPushButton;
class QTabWidget;

namespace speak5220::app {

class EncoderSettingsForm;

// What the Preferences dialog sets. A plain value type.
struct Preferences {
  ThemeChoice theme = ThemeChoice::kAuto;
  WaveformColours waveform_colours = WaveformColours::kDark;
  // The output device's id; empty for the system's default.
  QByteArray audio_device;
  // The settings new projects start from.
  core::encoder::EncoderOptions default_encoder;
  // The log level and file of the next start; empty for no file.
  core::LogLevel log_level = core::LogLevel::kInfo;
  QString log_file;

  friend bool operator==(const Preferences& a, const Preferences& b) = default;
};

// The preferences `store` holds.
Preferences ReadPreferences(IViewPreferences& store);
// Stores `preferences` in `store`.
void WritePreferences(const Preferences& preferences, IViewPreferences& store);

// The Preferences dialog, in four tabs.
// - Appearance: the theme (as View > Theme) and the waveforms' colours.
// - Audio: the output device, "System default" first; a device chosen
//   before that is not there now stays listed, marked as not connected.
// - New Projects: the settings new projects start from, in the encoder
//   panel's form (EncoderSettingsForm), against the encoder's defaults.
// - Logging: the level and the log file, which take effect at the next
//   start; --log-level and --log-file override them for one run.
// Restore Defaults puts every tab back to Preferences{}. The dialog only
// collects the choices (Chosen()); whoever opened it applies them. GUI
// thread only.
class PreferencesDialog : public QDialog {
  Q_OBJECT

 public:
  PreferencesDialog(const Preferences& current,
                    std::vector<AudioDeviceInfo> devices,
                    QWidget* parent = nullptr);

  // The preferences as the dialog shows them.
  Preferences Chosen() const;
  // Shows `preferences` in every tab.
  void Show(const Preferences& preferences);

  QTabWidget* Tabs() const { return tabs_; }
  QComboBox* ThemeBox() const { return theme_box_; }
  QComboBox* WaveformBox() const { return waveform_box_; }
  QComboBox* DeviceBox() const { return device_box_; }
  EncoderSettingsForm* EncoderForm() const { return encoder_form_; }
  QComboBox* LogLevelBox() const { return log_level_box_; }
  QLineEdit* LogFileField() const { return log_file_field_; }
  QPushButton* RestoreDefaultsButton() const { return restore_button_; }

 private:
  QWidget* MakeAppearanceTab();
  QWidget* MakeAudioTab();
  QWidget* MakeNewProjectsTab();
  QWidget* MakeLoggingTab();
  // Lists the devices, and `chosen` as not connected if it is none of them.
  void FillDevices(const QByteArray& chosen);
  void BrowseLogFile();

  std::vector<AudioDeviceInfo> devices_;
  QTabWidget* tabs_;
  QComboBox* theme_box_ = nullptr;
  QComboBox* waveform_box_ = nullptr;
  QComboBox* device_box_ = nullptr;
  EncoderSettingsForm* encoder_form_ = nullptr;
  QComboBox* log_level_box_ = nullptr;
  QLineEdit* log_file_field_ = nullptr;
  QPushButton* restore_button_ = nullptr;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PREFERENCES_PREFERENCES_DIALOG_H_
