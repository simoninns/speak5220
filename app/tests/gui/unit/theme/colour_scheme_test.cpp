/*
 * File:        colour_scheme_test.cpp
 * Module:      theme tests
 * Purpose:     Unit tests of following the desktop's colour scheme: the
 *              portal's values, and applying each preference
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/theme/colour_scheme.h"

#include <functional>
#include <utility>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

// A desktop whose preference the test sets and changes.
class FakeDesktopColourScheme : public IDesktopColourScheme {
 public:
  Qt::ColorScheme Preference() override { return preference; }
  void Watch(std::function<void(Qt::ColorScheme)> changed) override {
    watcher = std::move(changed);
  }

  void Change(Qt::ColorScheme scheme) {
    preference = scheme;
    watcher(scheme);
  }

  Qt::ColorScheme preference = Qt::ColorScheme::Unknown;
  std::function<void(Qt::ColorScheme)> watcher;
};

TEST(ColourSchemeTest, ReadsThePortalsValues) {
  EXPECT_EQ(SchemeFromPortalValue(0), Qt::ColorScheme::Unknown);
  EXPECT_EQ(SchemeFromPortalValue(1), Qt::ColorScheme::Dark);
  EXPECT_EQ(SchemeFromPortalValue(2), Qt::ColorScheme::Light);
  EXPECT_EQ(SchemeFromPortalValue(3), Qt::ColorScheme::Unknown);
}

TEST(ColourSchemeTest, FollowerAppliesThePreferenceAtOnce) {
  FakeDesktopColourScheme desktop;
  desktop.preference = Qt::ColorScheme::Dark;
  std::vector<Qt::ColorScheme> applied;
  const ColourSchemeFollower follower(
      desktop,
      [&applied](Qt::ColorScheme scheme) { applied.push_back(scheme); });
  EXPECT_EQ(applied, std::vector<Qt::ColorScheme>{Qt::ColorScheme::Dark});
}

TEST(ColourSchemeTest, FollowerAppliesEachChange) {
  FakeDesktopColourScheme desktop;
  std::vector<Qt::ColorScheme> applied;
  const ColourSchemeFollower follower(
      desktop,
      [&applied](Qt::ColorScheme scheme) { applied.push_back(scheme); });
  desktop.Change(Qt::ColorScheme::Dark);
  desktop.Change(Qt::ColorScheme::Light);
  desktop.Change(Qt::ColorScheme::Unknown);
  EXPECT_EQ(applied, (std::vector<Qt::ColorScheme>{
                         Qt::ColorScheme::Unknown, Qt::ColorScheme::Dark,
                         Qt::ColorScheme::Light, Qt::ColorScheme::Unknown}));
}

TEST(ColourSchemeTest, NoDesktopSourceHasNoPreference) {
  NoDesktopColourScheme desktop;
  EXPECT_EQ(desktop.Preference(), Qt::ColorScheme::Unknown);
}

}  // namespace
}  // namespace speak5220::app
