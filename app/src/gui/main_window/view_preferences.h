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
};

// IViewPreferences in the application's QSettings (the platform's usual
// place for preferences).
class SettingsViewPreferences : public IViewPreferences {
 public:
  static constexpr const char* kAdvancedSettingsKey = "advancedEncoderSettings";

  bool AdvancedSettingsShown() override;
  void SetAdvancedSettingsShown(bool shown) override;

 private:
  QSettings settings_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_MAIN_WINDOW_VIEW_PREFERENCES_H_
