/*
 * File:        colour_scheme_test.cpp
 * Module:      theme tests
 * Purpose:     Unit tests of following the desktop's colour scheme: the
 *              portal's values, the user's theme choice, and applying
 *              each preference
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/theme/colour_scheme.h"

#include <functional>
#include <optional>
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
      desktop, ThemeChoice::kAuto,
      [&applied](Qt::ColorScheme scheme) { applied.push_back(scheme); });
  EXPECT_EQ(applied, std::vector<Qt::ColorScheme>{Qt::ColorScheme::Dark});
}

TEST(ColourSchemeTest, FollowerAppliesEachChange) {
  FakeDesktopColourScheme desktop;
  std::vector<Qt::ColorScheme> applied;
  const ColourSchemeFollower follower(
      desktop, ThemeChoice::kAuto,
      [&applied](Qt::ColorScheme scheme) { applied.push_back(scheme); });
  desktop.Change(Qt::ColorScheme::Dark);
  desktop.Change(Qt::ColorScheme::Light);
  desktop.Change(Qt::ColorScheme::Unknown);
  EXPECT_EQ(applied, (std::vector<Qt::ColorScheme>{
                         Qt::ColorScheme::Unknown, Qt::ColorScheme::Dark,
                         Qt::ColorScheme::Light, Qt::ColorScheme::Unknown}));
}

TEST(ColourSchemeTest, ThemeChoiceKeysRoundTrip) {
  for (const ThemeChoice choice : kThemeChoices) {
    EXPECT_EQ(ThemeChoiceFromKey(ThemeChoiceKey(choice)), choice);
  }
  EXPECT_EQ(ThemeChoiceKey(ThemeChoice::kAuto), "auto");
  EXPECT_EQ(ThemeChoiceKey(ThemeChoice::kLight), "light");
  EXPECT_EQ(ThemeChoiceKey(ThemeChoice::kDark), "dark");
  EXPECT_FALSE(ThemeChoiceFromKey("sepia").has_value());
}

TEST(ColourSchemeTest, ChoiceOverridesTheDesktopUnlessAuto) {
  using Qt::ColorScheme;
  EXPECT_EQ(SchemeFor(ThemeChoice::kAuto, ColorScheme::Dark),
            ColorScheme::Dark);
  EXPECT_EQ(SchemeFor(ThemeChoice::kAuto, ColorScheme::Unknown),
            ColorScheme::Unknown);
  EXPECT_EQ(SchemeFor(ThemeChoice::kLight, ColorScheme::Dark),
            ColorScheme::Light);
  EXPECT_EQ(SchemeFor(ThemeChoice::kDark, ColorScheme::Unknown),
            ColorScheme::Dark);
}

TEST(ColourSchemeTest, FollowerAppliesTheChoiceAndIgnoresTheDesktopUntilAuto) {
  FakeDesktopColourScheme desktop;
  desktop.preference = Qt::ColorScheme::Dark;
  std::vector<Qt::ColorScheme> applied;
  ColourSchemeFollower follower(
      desktop, ThemeChoice::kLight,
      [&applied](Qt::ColorScheme scheme) { applied.push_back(scheme); });
  // Chosen light, so the desktop's changes are not applied, but kept.
  desktop.Change(Qt::ColorScheme::Unknown);
  desktop.Change(Qt::ColorScheme::Dark);
  follower.SetChoice(ThemeChoice::kDark);
  follower.SetChoice(ThemeChoice::kAuto);
  desktop.Change(Qt::ColorScheme::Light);
  EXPECT_EQ(follower.Choice(), ThemeChoice::kAuto);
  EXPECT_EQ(applied, (std::vector<Qt::ColorScheme>{
                         Qt::ColorScheme::Light, Qt::ColorScheme::Dark,
                         Qt::ColorScheme::Dark, Qt::ColorScheme::Light}));
}

TEST(ColourSchemeTest, NoDesktopSourceHasNoPreference) {
  NoDesktopColourScheme desktop;
  EXPECT_EQ(desktop.Preference(), Qt::ColorScheme::Unknown);
}

}  // namespace
}  // namespace speak5220::app
