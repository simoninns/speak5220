/*
 * File:        colour_scheme.h
 * Module:      theme
 * Purpose:     Follows the desktop's light or dark preference where Qt's
 *              platform theme does not report it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_THEME_COLOUR_SCHEME_H_
#define SPEAK5220_GUI_THEME_COLOUR_SCHEME_H_

#include <qnamespace.h>

#include <QtGlobal>
#include <functional>
#include <memory>

namespace speak5220::app {

// The desktop's colour-scheme preference, from a source Qt does not read
// itself. GUI thread only.
class IDesktopColourScheme {
 public:
  virtual ~IDesktopColourScheme() = default;

  // The preference now: Qt::ColorScheme::Unknown when the desktop states
  // none or cannot be asked.
  virtual Qt::ColorScheme Preference() = 0;
  // Calls `changed` with each new preference, on the GUI thread, for as
  // long as this object lives.
  virtual void Watch(std::function<void(Qt::ColorScheme)> changed) = 0;
};

// A source with no preference, for platforms where Qt follows the desktop
// itself (Windows, macOS).
class NoDesktopColourScheme : public IDesktopColourScheme {
 public:
  Qt::ColorScheme Preference() override { return Qt::ColorScheme::Unknown; }
  void Watch(std::function<void(Qt::ColorScheme)> /*changed*/) override {}
};

// XDG desktop portal, org.freedesktop.appearance color-scheme: 0 no
// preference, 1 prefers dark, 2 prefers light. Other values are no
// preference. Pure.
Qt::ColorScheme SchemeFromPortalValue(quint32 value);

// The source for this platform: the XDG desktop portal on Linux and the
// BSDs (on GNOME, Qt's default GTK platform theme reports the GTK theme's
// scheme, which stays light when the desktop prefers dark), and none
// elsewhere.
std::unique_ptr<IDesktopColourScheme> MakeDesktopColourScheme();

// Plan §5, "Colours": applies the desktop's preference at once and
// whenever it changes. A dark or light preference is requested from Qt
// (QStyleHints::setColorScheme()), which switches the style's palette and
// sends every widget a palette change; no preference leaves the choice to
// Qt (unsetColorScheme()). GUI thread only; the source must outlive the
// follower.
class ColourSchemeFollower {
 public:
  using Apply = std::function<void(Qt::ColorScheme)>;

  // `apply` applies a preference; the default is ApplyToApplication().
  explicit ColourSchemeFollower(IDesktopColourScheme& desktop,
                                Apply apply = ApplyToApplication);

  // Requests `scheme` from QStyleHints, or unsets the request for Unknown.
  static void ApplyToApplication(Qt::ColorScheme scheme);
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_THEME_COLOUR_SCHEME_H_
