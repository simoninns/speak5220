/*
 * File:        colour_scheme.cpp
 * Module:      theme
 * Purpose:     Follows the desktop's light or dark preference where Qt's
 *              platform theme does not report it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/theme/colour_scheme.h"

#include <QGuiApplication>
#include <QStyleHints>
#include <QtGlobal>
#include <functional>
#include <memory>
#include <utility>

#include "core/util/logging.h"

#ifdef SPEAK5220_HAVE_DESKTOP_PORTAL
#include "gui/theme/portal_colour_scheme.h"
#endif

namespace speak5220::app {

namespace {

// XDG desktop portal, org.freedesktop.appearance color-scheme.
constexpr quint32 kPortalPrefersDark = 1;
constexpr quint32 kPortalPrefersLight = 2;

const char* SchemeName(Qt::ColorScheme scheme) {
  switch (scheme) {
    case Qt::ColorScheme::Dark:
      return "dark";
    case Qt::ColorScheme::Light:
      return "light";
    case Qt::ColorScheme::Unknown:
      break;
  }
  return "no preference";
}

}  // namespace

Qt::ColorScheme SchemeFromPortalValue(quint32 value) {
  switch (value) {
    case kPortalPrefersDark:
      return Qt::ColorScheme::Dark;
    case kPortalPrefersLight:
      return Qt::ColorScheme::Light;
    default:
      return Qt::ColorScheme::Unknown;
  }
}

std::unique_ptr<IDesktopColourScheme> MakeDesktopColourScheme() {
#ifdef SPEAK5220_HAVE_DESKTOP_PORTAL
  return std::make_unique<PortalColourScheme>();
#else
  return std::make_unique<NoDesktopColourScheme>();
#endif
}

ColourSchemeFollower::ColourSchemeFollower(IDesktopColourScheme& desktop,
                                           Apply apply) {
  const Qt::ColorScheme scheme = desktop.Preference();
  core::Logger(core::kLogModuleGui)
      ->debug("desktop colour scheme: {}", SchemeName(scheme));
  apply(scheme);
  desktop.Watch([apply = std::move(apply)](Qt::ColorScheme changed) {
    core::Logger(core::kLogModuleGui)
        ->debug("desktop colour scheme changed: {}", SchemeName(changed));
    apply(changed);
  });
}

void ColourSchemeFollower::ApplyToApplication(Qt::ColorScheme scheme) {
  QStyleHints* hints = QGuiApplication::styleHints();
  if (scheme == Qt::ColorScheme::Unknown) {
    hints->unsetColorScheme();
  } else {
    hints->setColorScheme(scheme);
  }
}

}  // namespace speak5220::app
