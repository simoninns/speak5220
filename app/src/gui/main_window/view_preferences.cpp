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
#include <string>
#include <string_view>

#include "core/dsp/output_filter.h"

namespace speak5220::app {

bool SettingsViewPreferences::AdvancedSettingsShown() {
  return settings_.value(QString::fromLatin1(kAdvancedSettingsKey), false)
      .toBool();
}

void SettingsViewPreferences::SetAdvancedSettingsShown(bool shown) {
  settings_.setValue(QString::fromLatin1(kAdvancedSettingsKey), shown);
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

}  // namespace speak5220::app
