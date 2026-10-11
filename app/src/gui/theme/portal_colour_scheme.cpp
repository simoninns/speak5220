/*
 * File:        portal_colour_scheme.cpp
 * Module:      theme
 * Purpose:     The desktop's colour-scheme preference from the XDG desktop
 *              portal, over D-Bus
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/theme/portal_colour_scheme.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QDBusVariant>
#include <QList>
#include <QMetaType>
#include <QString>
#include <QVariant>
#include <QtGlobal>
#include <functional>
#include <optional>
#include <utility>

#include "core/util/logging.h"
#include "gui/theme/colour_scheme.h"

namespace speak5220::app {

namespace {

// XDG desktop portal, org.freedesktop.portal.Settings.
const char* const kService = "org.freedesktop.portal.Desktop";
const char* const kPath = "/org/freedesktop/portal/desktop";
const char* const kInterface = "org.freedesktop.portal.Settings";
const char* const kNamespace = "org.freedesktop.appearance";
const char* const kKey = "color-scheme";
// How long start-up waits for the portal, in milliseconds.
constexpr int kTimeoutMs = 1000;

// The value inside a reply: ReadOne() returns it in one variant, the older
// Read() in two.
std::optional<quint32> Unwrap(QVariant value) {
  while (value.metaType() == QMetaType::fromType<QDBusVariant>()) {
    value = value.value<QDBusVariant>().variant();
  }
  bool ok = false;
  const quint32 number = value.toUInt(&ok);
  return ok ? std::optional<quint32>(number) : std::nullopt;
}

std::optional<quint32> Ask(const char* method) {
  QDBusMessage call = QDBusMessage::createMethodCall(
      QString::fromLatin1(kService), QString::fromLatin1(kPath),
      QString::fromLatin1(kInterface), QString::fromLatin1(method));
  call << QString::fromLatin1(kNamespace) << QString::fromLatin1(kKey);
  const QDBusMessage reply =
      QDBusConnection::sessionBus().call(call, QDBus::Block, kTimeoutMs);
  if (reply.type() != QDBusMessage::ReplyMessage ||
      reply.arguments().isEmpty()) {
    core::Logger(core::kLogModuleGui)
        ->debug("desktop portal {}: {}", method,
                reply.errorMessage().toStdString());
    return std::nullopt;
  }
  return Unwrap(reply.arguments().constFirst());
}

}  // namespace

PortalColourScheme::PortalColourScheme(QObject* parent) : QObject(parent) {}

Qt::ColorScheme PortalColourScheme::Preference() {
  if (!QDBusConnection::sessionBus().isConnected()) {
    return Qt::ColorScheme::Unknown;
  }
  std::optional<quint32> value = Ask("ReadOne");
  if (!value.has_value()) {
    value = Ask("Read");
  }
  return value.has_value() ? SchemeFromPortalValue(*value)
                           : Qt::ColorScheme::Unknown;
}

void PortalColourScheme::Watch(std::function<void(Qt::ColorScheme)> changed) {
  changed_ = std::move(changed);
  if (connected_ || !QDBusConnection::sessionBus().isConnected()) {
    return;
  }
  connected_ = QDBusConnection::sessionBus().connect(
      QString::fromLatin1(kService), QString::fromLatin1(kPath),
      QString::fromLatin1(kInterface), QStringLiteral("SettingChanged"), this,
      SLOT(OnSettingChanged(QString, QString, QDBusVariant)));
}

void PortalColourScheme::OnSettingChanged(const QString& name_space,
                                          const QString& key,
                                          const QDBusVariant& value) {
  if (name_space != QLatin1String(kNamespace) || key != QLatin1String(kKey) ||
      !changed_) {
    return;
  }
  const std::optional<quint32> number = Unwrap(value.variant());
  changed_(number.has_value() ? SchemeFromPortalValue(*number)
                              : Qt::ColorScheme::Unknown);
}

}  // namespace speak5220::app
