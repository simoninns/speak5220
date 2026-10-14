/*
 * File:        main_window_phrom_import_test.cpp
 * Module:      main_window tests
 * Purpose:     Functional tests of File > Import PHROM Image: an exported
 *              image imports back as locked phrases with the same frames
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QCoreApplication>
#include <QDir>
#include <QPushButton>
#include <QString>
#include <QTemporaryDir>
#include <QUndoStack>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/encoder_panel.h"
#include "gui/export/export_dialog.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/main_window.h"
#include "gui/main_window/project_document.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/support/gui_mocks.h"

namespace speak5220::app {
namespace {

namespace fs = std::filesystem;
using ::testing::_;
using testing::FakeRecentFilesStore;
using testing::FakeViewPreferences;
using ::testing::HasSubstr;
using testing::MockDialogs;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestPlayback;

QString ProjectsDir() { return QStringLiteral(SPEAK5220_PROJECTS_DIR); }

class MainWindowPhromImportTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(directory_.isValid()); }

  QString Temp(const QString& name) const { return directory_.filePath(name); }

  // Opens `fixture` and exports its PHROMs as one file in the temporary
  // directory; returns the file and the project's phrases.
  QString ExportImages(const QString& fixture,
                       std::vector<core::project::Phrase>& phrases) {
    EXPECT_TRUE(window_.OpenProject(ProjectsDir() + fixture));
    phrases = window_.Document().Project().Phrases();
    EXPECT_CALL(dialogs_, AskExport(_))
        .WillOnce(Return(ExportRequest{.preset = {.name = "images",
                                                  .format = "acorn-phrom",
                                                  .scope = "all",
                                                  .layout = "concatenated"},
                                       .directory = directory_.path()}));
    window_.ExportProject();
    QCoreApplication::processEvents();
    EXPECT_EQ(window_.ExportsRunning(), 0);
    return Temp(QStringLiteral("images.bin"));
  }

  QTemporaryDir directory_;
  StrictMock<MockDialogs> dialogs_;
  FakeRecentFilesStore recent_;
  FakeViewPreferences preferences_;
  core::InlineTaskRunner runner_;
  TestPlayback playback_;
  testing::TestEncoding encoding_;
  MainWindow window_{dialogs_,         recent_,         preferences_,
                     runner_,          playback_.synth, playback_.player,
                     encoding_.Tools()};
};

TEST_F(MainWindowPhromImportTest, ExportedImagesImportBackAsLockedPhrases) {
  std::vector<core::project::Phrase> original;
  const QString image =
      ExportImages(QStringLiteral("/phrom/two-phroms"), original);
  ASSERT_TRUE(fs::exists(fs::path(image.toStdU16String())));
  ASSERT_TRUE(window_.NewProject());

  window_.ImportPhromImages({image}, 0);
  const core::project::Project& imported = window_.Document().Project();
  ASSERT_EQ(imported.Phrases().size(), original.size());
  for (std::size_t i = 0; i < original.size(); ++i) {
    const core::project::Phrase& phrase = imported.Phrases()[i];
    SCOPED_TRACE(phrase.name);
    EXPECT_EQ(phrase.name, original[i].name);
    EXPECT_TRUE(phrase.locked);
    EXPECT_FALSE(phrase.source.has_value());
    ASSERT_TRUE(phrase.encoding.has_value());
    ASSERT_TRUE(original[i].encoding.has_value());
    EXPECT_EQ(phrase.encoding->frames, original[i].encoding->frames);
  }
  // One undoable step, and the imported words cannot be re-encoded.
  EXPECT_EQ(window_.Document().UndoStack().count(), 1);
  window_.Panel()->Select({imported.Phrases().front().id});
  EXPECT_FALSE(window_.Encoder()->ReEncodeButton()->isEnabled());
  // Imported into a new project, each image's header strings come too, so
  // the images export again as they were; undo takes them away with the
  // words.
  EXPECT_EQ(imported.Settings().phroms.size(),
            static_cast<std::size_t>(imported.Allocation().PhromsUsed()));
  window_.Document().UndoStack().undo();
  EXPECT_EQ(window_.Model().PhraseCount(), 0);
  EXPECT_TRUE(window_.Document().Project().Settings().phroms.empty());
}

TEST_F(MainWindowPhromImportTest, NamesTakenAlreadyAreMadeUnique) {
  std::vector<core::project::Phrase> original;
  const QString image =
      ExportImages(QStringLiteral("/phrom/one-phrom"), original);
  // Imported into the project it came from, every name is taken.
  window_.ImportPhromImages({image}, 0);
  const core::project::Project& project = window_.Document().Project();
  EXPECT_EQ(project.Phrases().size(), 2 * original.size());
  EXPECT_EQ(project.PhraseNames().size(), project.Phrases().size());
}

TEST_F(MainWindowPhromImportTest, UnreadableImagesAreReportedAndTheRestAdded) {
  std::vector<core::project::Phrase> original;
  const QString image =
      ExportImages(QStringLiteral("/phrom/one-phrom"), original);
  ASSERT_TRUE(window_.NewProject());
  const QString short_file = Temp(QStringLiteral("short.bin"));
  {
    std::ofstream file(fs::path(short_file.toStdU16String()), std::ios::binary);
    file << std::string(1000, '\0');
  }
  const QString missing = Temp(QStringLiteral("missing.bin"));
  EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Import PHROM Images"), _))
      .WillOnce([&](const QString&, const QString& message) {
        EXPECT_THAT(
            message.toStdString(),
            HasSubstr(QDir::toNativeSeparators(short_file).toStdString() +
                      ": a PHROM image is a multiple of 16384"));
        EXPECT_THAT(message.toStdString(),
                    HasSubstr(QDir::toNativeSeparators(missing).toStdString() +
                              ": cannot be read"));
      });
  window_.ImportPhromImages({short_file, image, missing}, 0);
  EXPECT_EQ(window_.Document().Project().Phrases().size(), original.size());
}

TEST_F(MainWindowPhromImportTest, ImportsFromTheChosenFiles) {
  std::vector<core::project::Phrase> original;
  const QString image =
      ExportImages(QStringLiteral("/phrom/one-phrom"), original);
  ASSERT_TRUE(window_.NewProject());
  EXPECT_CALL(dialogs_, ImportPhromPaths())
      .WillOnce(Return(QStringList{image}));
  for (QAction* action : window_.findChildren<QAction*>()) {
    if (action->text() == QStringLiteral("&Import PHROM Image...")) {
      action->trigger();
    }
  }
  EXPECT_EQ(window_.Document().Project().Phrases().size(), original.size());
}

}  // namespace
}  // namespace speak5220::app
