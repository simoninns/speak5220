/*
 * File:        recent_files_test.cpp
 * Module:      main_window tests
 * Purpose:     Unit tests for the recent-file list
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/recent_files.h"

#include <QString>
#include <QStringList>

#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

TEST(RecentFilesTest, AddPutsThePathFirstWithoutRepeats) {
  RecentFiles recent;
  recent.Add(QStringLiteral("/a.s5proj"));
  recent.Add(QStringLiteral("/b.s5proj"));
  recent.Add(QStringLiteral("/a.s5proj"));
  EXPECT_EQ(recent.Paths(), (QStringList{QStringLiteral("/a.s5proj"),
                                         QStringLiteral("/b.s5proj")}));
}

TEST(RecentFilesTest, KeepsTheNewestEight) {
  RecentFiles recent;
  for (int i = 0; i < 10; ++i) {
    recent.Add(QStringLiteral("/%1.s5proj").arg(i));
  }
  ASSERT_EQ(recent.Paths().size(), RecentFiles::kMaxEntries);
  EXPECT_EQ(recent.Paths().front(), QStringLiteral("/9.s5proj"));
  EXPECT_EQ(recent.Paths().back(), QStringLiteral("/2.s5proj"));
}

TEST(RecentFilesTest, StoredListIsTidied) {
  QStringList stored = {QStringLiteral("/a"), QString(), QStringLiteral("/a"),
                        QStringLiteral("/b")};
  for (int i = 0; i < 10; ++i) {
    stored << QStringLiteral("/%1").arg(i);
  }
  const RecentFiles recent(stored);
  ASSERT_EQ(recent.Paths().size(), RecentFiles::kMaxEntries);
  EXPECT_EQ(recent.Paths().mid(0, 3),
            (QStringList{QStringLiteral("/a"), QStringLiteral("/b"),
                         QStringLiteral("/0")}));
}

TEST(RecentFilesTest, RemoveAndClear) {
  RecentFiles recent({QStringLiteral("/a"), QStringLiteral("/b")});
  recent.Remove(QStringLiteral("/a"));
  EXPECT_EQ(recent.Paths(), QStringList{QStringLiteral("/b")});
  recent.Clear();
  EXPECT_TRUE(recent.Paths().isEmpty());
}

}  // namespace
}  // namespace speak5220::app
