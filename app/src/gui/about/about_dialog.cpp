/*
 * File:        about_dialog.cpp
 * Module:      about
 * Purpose:     The About box: the version, the licence and the third-party
 *              notices
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/about/about_dialog.h"

#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFontDatabase>
#include <QIODevice>
#include <QString>
#include <QTabWidget>
#include <QTextBrowser>
#include <QVBoxLayout>
#include <QtGlobal>
#include <string_view>

#include "core/util/version.h"
#include "gui/about/third_party_notices.h"

namespace speak5220::app {

namespace {

// The dialog's initial size in device-independent pixels, wide enough for
// the licence's 80 columns.
constexpr int kInitialWidth = 680;
constexpr int kInitialHeight = 560;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QTextBrowser* Browser(QWidget* parent) {
  auto* browser = new QTextBrowser(parent);
  browser->setOpenExternalLinks(true);
  return browser;
}

}  // namespace

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent),
      tabs_(new QTabWidget(this)),
      about_(Browser(this)),
      licence_(Browser(this)),
      notices_(Browser(this)) {
  setWindowTitle(QStringLiteral("About Speak5220"));
  resize(kInitialWidth, kInitialHeight);

  // The logo heads the text, at its width in device-independent pixels; Qt
  // picks the @2x image on a high-density screen.
  about_->setAccessibleName(QStringLiteral("About Speak5220"));
  about_->setHtml(
      QStringLiteral(
          "<p><img src=\":/speak5220/speak5220-logo.png\"></p>"
          "<p><b>Speak5220</b> %1</p>"
          "<p>Creates TMS5220 speech data and builds PHROMs.</p>"
          "<p>Copyright &copy; 2026 Simon Inns</p>"
          "<p>This program is free software: you can redistribute it "
          "and/or modify it under the terms of the GNU General Public "
          "License as published by the Free Software Foundation, either "
          "version 3 of the License, or (at your option) any later "
          "version.</p>"
          "<p>This program is distributed in the hope that it will be "
          "useful, but WITHOUT ANY WARRANTY; without even the implied "
          "warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR "
          "PURPOSE. See the Licence tab for more details.</p>"
          "<p>Built with Qt %2, running on Qt %3.</p>"
          "<p><a href=\"https://simoninns.github.io/speak5220/\">"
          "https://simoninns.github.io/speak5220/</a></p>")
          .arg(ToQString(core::Version()).toHtmlEscaped(),
               QStringLiteral(QT_VERSION_STR),
               QString::fromLatin1(qVersion())));

  licence_->setAccessibleName(QStringLiteral("Licence"));
  QFile licence(QString::fromLatin1(kLicenceResource));
  if (licence.open(QIODevice::ReadOnly | QIODevice::Text)) {
    licence_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
    licence_->setPlainText(QString::fromUtf8(licence.readAll()));
  } else {
    licence_->setHtml(QStringLiteral(
        "<p>Speak5220 is licensed under the GNU General Public License, "
        "version 3 or later: <a href=\"https://www.gnu.org/licenses/"
        "gpl-3.0.html\">https://www.gnu.org/licenses/gpl-3.0.html</a></p>"));
  }

  notices_->setAccessibleName(QStringLiteral("Third-party software"));
  notices_->setHtml(ThirdPartyNoticesHtml());

  tabs_->addTab(about_, QStringLiteral("&About"));
  tabs_->addTab(licence_, QStringLiteral("&Licence"));
  tabs_->addTab(notices_, QStringLiteral("&Third-Party Software"));

  auto* buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
  connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
  auto* layout = new QVBoxLayout(this);
  layout->addWidget(tabs_, 1);
  layout->addWidget(buttons);
}

}  // namespace speak5220::app
