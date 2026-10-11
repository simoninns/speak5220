/*
 * File:        gui_mocks.h
 * Module:      gui tests
 * Purpose:     A gMock mock of the main window's dialogs and an in-memory
 *              recent-file store
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_GUI_SUPPORT_GUI_MOCKS_H_
#define SPEAK5220_TESTS_GUI_SUPPORT_GUI_MOCKS_H_

#include <QString>
#include <QStringList>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/project/phrase.h"
#include "gmock/gmock.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/recent_files.h"

namespace speak5220::testing {

class MockDialogs : public app::IDialogs {
 public:
  MOCK_METHOD(std::optional<QString>, OpenProjectPath, (), (override));
  MOCK_METHOD(std::optional<QString>, SaveProjectPath,
              (const QString& suggested), (override));
  MOCK_METHOD(QStringList, ImportFilePaths, (), (override));
  MOCK_METHOD(app::SaveChoice, AskSaveChanges, (const QString& name),
              (override));
  MOCK_METHOD(bool, ConfirmRemove, (const QStringList& names), (override));
  MOCK_METHOD(bool, AskNormalise, (const QStringList& files), (override));
  MOCK_METHOD(void, ShowError, (const QString& title, const QString& message),
              (override));
};

// Keeps the list in memory.
class FakeRecentFilesStore : public app::IRecentFilesStore {
 public:
  QStringList Load() override { return paths; }
  void Save(const QStringList& saved) override { paths = saved; }

  QStringList paths;
};

// Matches a span of phrase ids against a list (std::span has no
// const_iterator in C++20, which gMock's container matchers need).
inline auto IdsAre(std::vector<core::project::PhraseId> expected) {
  return ::testing::ResultOf(
      "the ids",
      [](std::span<const core::project::PhraseId> ids) {
        return std::vector<core::project::PhraseId>(ids.begin(), ids.end());
      },
      ::testing::Eq(std::move(expected)));
}

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_GUI_SUPPORT_GUI_MOCKS_H_
