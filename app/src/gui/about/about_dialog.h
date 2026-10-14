/*
 * File:        about_dialog.h
 * Module:      about
 * Purpose:     The About box: the version, the licence and the third-party
 *              notices
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ABOUT_ABOUT_DIALOG_H_
#define SPEAK5220_GUI_ABOUT_ABOUT_DIALOG_H_

#include <QDialog>

class QTabWidget;
class QTextBrowser;

namespace speak5220::app {

// Plan Phase 12: Help > About, in three tabs. About: the logo, the
// version, the Qt version in use and the GPL's notice. Licence: the GNU
// GPL version 3, from the application's resources (kLicenceResource), or a
// link to it when the resources lack it. Third-Party Software: every
// library and ported code with its licence (ThirdPartyNoticesHtml()).
// Links open in the browser. GUI thread only.
class AboutDialog : public QDialog {
  Q_OBJECT

 public:
  // The GPL's text among the application's resources.
  static constexpr const char* kLicenceResource =
      ":/speak5220/licences/GPL-3.0.txt";

  explicit AboutDialog(QWidget* parent = nullptr);

  QTabWidget* Tabs() const { return tabs_; }
  QTextBrowser* AboutText() const { return about_; }
  QTextBrowser* LicenceText() const { return licence_; }
  QTextBrowser* NoticesText() const { return notices_; }

 private:
  QTabWidget* tabs_;
  QTextBrowser* about_;
  QTextBrowser* licence_;
  QTextBrowser* notices_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ABOUT_ABOUT_DIALOG_H_
