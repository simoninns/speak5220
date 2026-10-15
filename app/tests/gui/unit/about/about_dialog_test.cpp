/*
 * File:        about_dialog_test.cpp
 * Module:      about tests
 * Purpose:     Unit tests of the About box and its third-party notices
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/about/about_dialog.h"

#include <QString>
#include <QTabWidget>
#include <QTextBrowser>
#include <set>
#include <string>
#include <string_view>

#include "core/util/version.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/about/third_party_notices.h"

namespace speak5220::app {
namespace {

using ::testing::IsSupersetOf;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

TEST(ThirdPartyNoticesTest, ListsEveryLibraryOfThePlan) {
  // The third-party libraries the application and its tests use.
  std::set<std::string> names;
  for (const ThirdPartyNotice& notice : ThirdPartyNotices()) {
    names.emplace(notice.name);
  }
  EXPECT_THAT(
      names,
      IsSupersetOf({"Qt 6", "libsndfile", "libsamplerate", "pocketfft (C++)",
                    "libzip", "yaml-cpp", "libfvad", "SPTK 4",
                    "REAPER (SPTK 4's copy)", "WORLD's Harvest (SPTK 4's copy)",
                    "spdlog", "GoogleTest (with Google Mock)", "pystoi"}));
}

TEST(ThirdPartyNoticesTest, EveryEntryHasALicenceCopyrightAndHome) {
  for (const ThirdPartyNotice& notice : ThirdPartyNotices()) {
    SCOPED_TRACE(std::string(notice.name));
    EXPECT_FALSE(notice.use.empty());
    EXPECT_FALSE(notice.licence.empty());
    EXPECT_FALSE(notice.copyright.empty());
    EXPECT_TRUE(notice.url.starts_with("https://"));
  }
}

TEST(ThirdPartyNoticesTest, ShortLicencesAreGivenInFull) {
  for (const ThirdPartyNotice& notice : ThirdPartyNotices()) {
    if (notice.licence.starts_with("BSD") || notice.licence == "MIT") {
      EXPECT_FALSE(LicenceText(notice.licence).empty()) << notice.licence;
    }
  }
  EXPECT_TRUE(LicenceText("Apache-2.0").empty());
  EXPECT_EQ(LicenceUrl("Apache-2.0"),
            QStringLiteral("https://spdx.org/licenses/Apache-2.0.html"));
}

TEST(ThirdPartyNoticesTest, HtmlNamesEveryEntryAndItsLicence) {
  const QString html = ThirdPartyNoticesHtml();
  for (const ThirdPartyNotice& notice : ThirdPartyNotices()) {
    EXPECT_TRUE(html.contains(ToQString(notice.name).toHtmlEscaped()))
        << notice.name;
    EXPECT_TRUE(html.contains(ToQString(notice.licence))) << notice.licence;
  }
  EXPECT_TRUE(html.contains(QStringLiteral("Permission is hereby granted")));
  EXPECT_TRUE(html.contains(QStringLiteral("Neither the name")));
}

TEST(AboutDialogTest, ShowsTheVersionTheLicenceAndTheNotices) {
  const AboutDialog dialog;
  EXPECT_EQ(dialog.Tabs()->count(), 3);
  EXPECT_TRUE(
      dialog.AboutText()->toPlainText().contains(ToQString(core::Version())));
  EXPECT_TRUE(dialog.AboutText()->toPlainText().contains(
      QStringLiteral("GNU General Public License")));
  // The tests have no resources, so the licence is linked.
  EXPECT_TRUE(
      dialog.LicenceText()->toPlainText().contains(QStringLiteral("gpl-3.0")));
  EXPECT_TRUE(
      dialog.NoticesText()->toPlainText().contains(QStringLiteral("libzip")));
  EXPECT_FALSE(dialog.NoticesText()->accessibleName().isEmpty());
}

}  // namespace
}  // namespace speak5220::app
