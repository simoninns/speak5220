/*
 * File:        portal_colour_scheme.h
 * Module:      theme
 * Purpose:     The desktop's colour-scheme preference from the XDG desktop
 *              portal, over D-Bus
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_THEME_PORTAL_COLOUR_SCHEME_H_
#define SPEAK5220_GUI_THEME_PORTAL_COLOUR_SCHEME_H_

#include <QObject>
#include <QString>
#include <QtGlobal>
#include <functional>

#include "gui/theme/colour_scheme.h"

class QDBusVariant;

namespace speak5220::app {

// IDesktopColourScheme over the XDG desktop portal's Settings interface
// (org.freedesktop.appearance color-scheme), which GNOME, KDE Plasma and
// other desktops provide. Preference() asks the portal, waiting at most
// a second; with no portal it is Unknown. Watch() follows the portal's
// SettingChanged signal. A thin adapter over Qt D-Bus, checked by hand
// (plan Phase 6). GUI thread only.
class PortalColourScheme : public QObject, public IDesktopColourScheme {
  Q_OBJECT

 public:
  explicit PortalColourScheme(QObject* parent = nullptr);

  Qt::ColorScheme Preference() override;
  void Watch(std::function<void(Qt::ColorScheme)> changed) override;

 private slots:
  void OnSettingChanged(const QString& name_space, const QString& key,
                        const QDBusVariant& value);

 private:
  std::function<void(Qt::ColorScheme)> changed_;
  bool connected_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_THEME_PORTAL_COLOUR_SCHEME_H_
