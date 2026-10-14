/*
 * File:        preferences_dialog.cpp
 * Module:      preferences
 * Purpose:     The Preferences dialog: appearance, the audio device, the
 *              settings new projects start from, and logging
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/preferences/preferences_dialog.h"

#include <QByteArray>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QVariant>
#include <QWidget>
#include <array>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/util/logging.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/main_window/view_preferences.h"
#include "gui/playback/audio_output.h"
#include "gui/theme/colour_scheme.h"
#include "gui/theme/colours.h"

namespace speak5220::app {

namespace {

// The levels in the Logging tab's order.
constexpr std::array<core::LogLevel, 3> kLogLevels = {
    core::LogLevel::kInfo, core::LogLevel::kDebug, core::LogLevel::kTrace};

QString ThemeLabel(ThemeChoice choice) {
  switch (choice) {
    case ThemeChoice::kAuto:
      return QStringLiteral("Follow the desktop");
    case ThemeChoice::kLight:
      return QStringLiteral("Light");
    case ThemeChoice::kDark:
      return QStringLiteral("Dark");
  }
  return {};
}

QString WaveformLabel(WaveformColours choice) {
  switch (choice) {
    case WaveformColours::kDark:
      return QStringLiteral("Dark, in either theme");
    case WaveformColours::kTheme:
      return QStringLiteral("As the theme");
  }
  return {};
}

QString LevelLabel(core::LogLevel level) {
  switch (level) {
    case core::LogLevel::kInfo:
      return QStringLiteral("Info: what the application does");
    case core::LogLevel::kDebug:
      return QStringLiteral("Debug: and the settings and decisions behind it");
    case core::LogLevel::kTrace:
      return QStringLiteral("Trace: and every frame and sample (large)");
  }
  return {};
}

// A note under a tab's settings, wrapped.
QLabel* Note(const QString& text, QWidget* parent) {
  auto* label = new QLabel(text, parent);
  label->setWordWrap(true);
  return label;
}

// Selects the item whose data is `value`, if there is one.
void SelectData(QComboBox* box, const QVariant& value) {
  const int index = box->findData(value);
  if (index >= 0) {
    box->setCurrentIndex(index);
  }
}

}  // namespace

Preferences ReadPreferences(IViewPreferences& store) {
  return {.theme = store.ChosenTheme(),
          .waveform_colours = store.ChosenWaveformColours(),
          .audio_device = store.ChosenAudioDevice(),
          .default_encoder = store.ChosenDefaultEncoder(),
          .log_level = store.ChosenLogLevel(),
          .log_file = store.ChosenLogFile()};
}

void WritePreferences(const Preferences& preferences, IViewPreferences& store) {
  store.SetTheme(preferences.theme);
  store.SetWaveformColours(preferences.waveform_colours);
  store.SetAudioDevice(preferences.audio_device);
  store.SetDefaultEncoder(preferences.default_encoder);
  store.SetLogLevel(preferences.log_level);
  store.SetLogFile(preferences.log_file);
}

PreferencesDialog::PreferencesDialog(const Preferences& current,
                                     std::vector<AudioDeviceInfo> devices,
                                     QWidget* parent)
    : QDialog(parent),
      devices_(std::move(devices)),
      tabs_(new QTabWidget(this)) {
  setWindowTitle(QStringLiteral("Preferences"));
  tabs_->addTab(MakeAppearanceTab(), QStringLiteral("&Appearance"));
  tabs_->addTab(MakeAudioTab(), QStringLiteral("A&udio"));
  tabs_->addTab(MakeNewProjectsTab(), QStringLiteral("&New Projects"));
  tabs_->addTab(MakeLoggingTab(), QStringLiteral("&Logging"));

  auto* buttons =
      new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel |
                               QDialogButtonBox::RestoreDefaults,
                           this);
  restore_button_ = buttons->button(QDialogButtonBox::RestoreDefaults);
  connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  connect(restore_button_, &QPushButton::clicked, this,
          [this] { Show(Preferences{}); });

  auto* layout = new QVBoxLayout(this);
  layout->addWidget(tabs_, 1);
  layout->addWidget(buttons);
  Show(current);
}

QWidget* PreferencesDialog::MakeAppearanceTab() {
  auto* tab = new QWidget(this);
  auto* form = new QFormLayout(tab);
  theme_box_ = new QComboBox(tab);
  for (const ThemeChoice choice : kThemeChoices) {
    theme_box_->addItem(ThemeLabel(choice), static_cast<int>(choice));
  }
  theme_box_->setToolTip(
      QStringLiteral("Light or dark, or as the desktop prefers (also in "
                     "View > Theme)"));
  form->addRow(QStringLiteral("&Theme:"), theme_box_);
  waveform_box_ = new QComboBox(tab);
  for (const WaveformColours choice : kWaveformColourChoices) {
    waveform_box_->addItem(WaveformLabel(choice), static_cast<int>(choice));
  }
  waveform_box_->setToolTip(
      QStringLiteral("Draw the waveforms on black whatever the theme, or "
                     "in the theme's own colours"));
  form->addRow(QStringLiteral("&Waveforms:"), waveform_box_);
  return tab;
}

QWidget* PreferencesDialog::MakeAudioTab() {
  auto* tab = new QWidget(this);
  auto* layout = new QVBoxLayout(tab);
  auto* form = new QFormLayout();
  device_box_ = new QComboBox(tab);
  device_box_->setToolTip(
      QStringLiteral("The device the phrases are played on"));
  form->addRow(QStringLiteral("&Output device:"), device_box_);
  layout->addLayout(form);
  layout->addWidget(
      Note(QStringLiteral("If the chosen device is disconnected, the "
                          "phrases play on the system's default."),
           tab));
  layout->addStretch(1);
  return tab;
}

QWidget* PreferencesDialog::MakeNewProjectsTab() {
  auto* tab = new QWidget(this);
  auto* layout = new QVBoxLayout(tab);
  layout->addWidget(Note(
      QStringLiteral("New projects start from these settings. A project "
                     "keeps the settings it was made with, and each phrase "
                     "can change them on its Preprocess and Encode tabs."),
      tab));
  encoder_form_ = new EncoderSettingsForm(tab);
  auto* scroll = new QScrollArea(tab);
  scroll->setWidgetResizable(true);
  scroll->setFrameShape(QFrame::NoFrame);
  scroll->setWidget(encoder_form_);
  layout->addWidget(scroll, 1);
  return tab;
}

QWidget* PreferencesDialog::MakeLoggingTab() {
  auto* tab = new QWidget(this);
  auto* layout = new QVBoxLayout(tab);
  auto* form = new QFormLayout();
  log_level_box_ = new QComboBox(tab);
  for (const core::LogLevel level : kLogLevels) {
    log_level_box_->addItem(LevelLabel(level), static_cast<int>(level));
  }
  form->addRow(QStringLiteral("Log &level:"), log_level_box_);
  auto* file_row = new QHBoxLayout();
  log_file_field_ = new QLineEdit(tab);
  log_file_field_->setPlaceholderText(QStringLiteral("No log file"));
  log_file_field_->setClearButtonEnabled(true);
  log_file_field_->setToolTip(
      QStringLiteral("Also write the log to this file, replacing it at "
                     "each start"));
  auto* browse = new QPushButton(QStringLiteral("&Browse..."), tab);
  browse->setToolTip(QStringLiteral("Choose the log file"));
  connect(browse, &QPushButton::clicked, this,
          &PreferencesDialog::BrowseLogFile);
  file_row->addWidget(log_file_field_, 1);
  file_row->addWidget(browse);
  form->addRow(QStringLiteral("Log &file:"), file_row);
  // QFormLayout gives a label a buddy only for a field that is a widget.
  if (auto* label = qobject_cast<QLabel*>(form->labelForField(file_row));
      label != nullptr) {
    label->setBuddy(log_file_field_);
  }
  layout->addLayout(form);
  layout->addWidget(Note(
      QStringLiteral("These take effect the next time Speak5220 starts. "
                     "The --log-level and --log-file options override them "
                     "for one run. Warnings and errors are always logged."),
      tab));
  layout->addStretch(1);
  return tab;
}

void PreferencesDialog::FillDevices(const QByteArray& chosen) {
  device_box_->clear();
  device_box_->addItem(QStringLiteral("System default"), QByteArray());
  bool found = chosen.isEmpty();
  for (const AudioDeviceInfo& device : devices_) {
    device_box_->addItem(device.description, device.id);
    found = found || device.id == chosen;
  }
  if (!found) {
    device_box_->addItem(
        QStringLiteral("%1 (not connected)").arg(QString::fromUtf8(chosen)),
        chosen);
  }
  SelectData(device_box_, chosen);
}

void PreferencesDialog::Show(const Preferences& preferences) {
  SelectData(theme_box_, static_cast<int>(preferences.theme));
  SelectData(waveform_box_, static_cast<int>(preferences.waveform_colours));
  FillDevices(preferences.audio_device);
  encoder_form_->SetOptions(preferences.default_encoder,
                            core::encoder::EncoderOptions{},
                            QStringLiteral("the encoder's default"));
  SelectData(log_level_box_, static_cast<int>(preferences.log_level));
  log_file_field_->setText(preferences.log_file);
}

Preferences PreferencesDialog::Chosen() const {
  return {
      .theme = static_cast<ThemeChoice>(theme_box_->currentData().toInt()),
      .waveform_colours =
          static_cast<WaveformColours>(waveform_box_->currentData().toInt()),
      .audio_device = device_box_->currentData().toByteArray(),
      .default_encoder = encoder_form_->Options(),
      .log_level =
          static_cast<core::LogLevel>(log_level_box_->currentData().toInt()),
      .log_file = log_file_field_->text().trimmed()};
}

void PreferencesDialog::BrowseLogFile() {
  const QString start =
      log_file_field_->text().isEmpty()
          ? QDir::home().filePath(QStringLiteral("speak5220.log"))
          : log_file_field_->text();
  const QString path = QFileDialog::getSaveFileName(
      this, QStringLiteral("Log File"), start,
      QStringLiteral("Log files (*.log *.txt);;All files (*)"));
  if (!path.isEmpty()) {
    log_file_field_->setText(QDir::toNativeSeparators(path));
  }
}

}  // namespace speak5220::app
