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
#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <string_view>

namespace speak5220::app {

// The theme the user chose (View > Theme): the desktop's light or dark
// preference, or always light, or always dark.
enum class ThemeChoice { kAuto, kLight, kDark };

// Every choice, in the menu's order.
inline constexpr std::array<ThemeChoice, 3> kThemeChoices = {
    ThemeChoice::kAuto, ThemeChoice::kLight, ThemeChoice::kDark};

// The choice's key for storing it ("auto", "light", "dark"). Pure.
std::string_view ThemeChoiceKey(ThemeChoice choice);
// The choice with `key`, or none for a key no choice has. Pure.
std::optional<ThemeChoice> ThemeChoiceFromKey(std::string_view key);
// The scheme to request from Qt for `choice` while the desktop prefers
// `desktop`: Light or Dark as chosen, or the desktop's preference for
// Auto (Unknown leaving it to Qt). Pure.
Qt::ColorScheme SchemeFor(ThemeChoice choice, Qt::ColorScheme desktop);

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

// Plan §5, "Colours": applies the user's theme choice at once and
// whenever it changes, and for Auto the desktop's preference, at once and
// whenever that changes (SchemeFor()). A dark or light scheme is requested
// from Qt (QStyleHints::setColorScheme()), which switches the style's
// palette and sends every widget a palette change; no preference leaves
// the choice to Qt (unsetColorScheme()). GUI thread only; the source must
// outlive the follower.
class ColourSchemeFollower {
 public:
  using Apply = std::function<void(Qt::ColorScheme)>;

  // `apply` applies a scheme; the default is ApplyToApplication().
  ColourSchemeFollower(IDesktopColourScheme& desktop, ThemeChoice choice,
                       Apply apply = ApplyToApplication);
  // The desktop's watcher refers to this object, which so cannot move.
  ColourSchemeFollower(const ColourSchemeFollower&) = delete;
  ColourSchemeFollower& operator=(const ColourSchemeFollower&) = delete;
  ColourSchemeFollower(ColourSchemeFollower&&) = delete;
  ColourSchemeFollower& operator=(ColourSchemeFollower&&) = delete;
  ~ColourSchemeFollower() = default;

  ThemeChoice Choice() const { return choice_; }
  // Applies `choice`.
  void SetChoice(ThemeChoice choice);

  // Requests `scheme` from QStyleHints, or unsets the request for Unknown.
  static void ApplyToApplication(Qt::ColorScheme scheme);

 private:
  void ApplyNow();

  Apply apply_;
  ThemeChoice choice_;
  Qt::ColorScheme desktop_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_THEME_COLOUR_SCHEME_H_
