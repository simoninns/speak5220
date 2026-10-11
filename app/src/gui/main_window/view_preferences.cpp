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

namespace speak5220::app {

bool SettingsViewPreferences::AdvancedSettingsShown() {
  return settings_.value(QString::fromLatin1(kAdvancedSettingsKey), false)
      .toBool();
}

void SettingsViewPreferences::SetAdvancedSettingsShown(bool shown) {
  settings_.setValue(QString::fromLatin1(kAdvancedSettingsKey), shown);
}

}  // namespace speak5220::app
