/*
 * File:        main_window_project_test.cpp
 * Module:      main_window tests
 * Purpose:     Functional tests of the main window with real files:
 *              opening, saving, closing with unsaved changes and importing
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QAction>
#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QMimeData>
#include <QModelIndex>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QUndoStack>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <memory>
#include <numbers>
#include <optional>
#include <string>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/audio/wav_io.h"
#include "core/project/phrase.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/export/export_dialog.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/main_window.h"
#include "gui/main_window/project_document.h"
#include "gui/phrase_list/phrase_list_model.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/support/gui_mocks.h"
#include "gui/undo/project_edits.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

namespace fs = std::filesystem;
using ::testing::_;
using testing::AudioPhrase;
using testing::FakeRecentFilesStore;
using testing::FakeViewPreferences;
using ::testing::HasSubstr;
using testing::MockDialogs;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestPlayback;

// How long an import may take before a test gives up.
constexpr qint64 kImportTimeoutMs = 20000;

QString ProjectsDir() { return QStringLiteral(SPEAK5220_PROJECTS_DIR); }

fs::path ToPath(const QString& text) { return {text.toStdU16String()}; }

// Writes a one-second tone at `rate` with peak `amplitude`.
void WriteTone(const QString& path, int rate, float amplitude) {
  constexpr double kFrequency = 220.0;
  std::vector<float> samples(static_cast<std::size_t>(rate));
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = amplitude * static_cast<float>(
                                 std::sin(2.0 * std::numbers::pi * kFrequency *
                                          static_cast<double>(i) / rate));
  }
  core::audio::SndfileWavWriter writer;
  ASSERT_TRUE(writer
                  .Write(ToPath(path), samples, rate,
                         core::audio::WavOutputFormat::kPcm16)
                  .IsOk());
}

float Peak(const std::vector<float>& audio) {
  float peak = 0.0F;
  for (const float sample : audio) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

// Waits, handling events, until the window has no import running.
void WaitForImports(MainWindow& window) {
  QElapsedTimer timer;
  timer.start();
  QCoreApplication::processEvents();
  while (window.ImportsRunning() > 0 && timer.elapsed() < kImportTimeoutMs) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  ASSERT_EQ(window.ImportsRunning(), 0) << "the import did not finish";
}

// Drops files on the phrase list before `row`.
void DropFiles(MainWindow& window, const QStringList& paths, int row) {
  QMimeData data;
  QList<QUrl> urls;
  for (const QString& path : paths) {
    urls << QUrl::fromLocalFile(path);
  }
  data.setUrls(urls);
  ASSERT_TRUE(window.Model().dropMimeData(&data, Qt::CopyAction, row, 0,
                                          QModelIndex()));
}

class MainWindowProjectTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(directory_.isValid()); }

  QString Temp(const QString& name) const { return directory_.filePath(name); }

  // A copy of a fixture project in the temporary directory.
  QString CopyFixture(const QString& fixture) {
    const QString target = Temp(QDir(fixture).dirName());
    fs::copy(ToPath(ProjectsDir() + QLatin1Char('/') + fixture), ToPath(target),
             fs::copy_options::recursive);
    return target;
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

TEST_F(MainWindowProjectTest, OpensAFixtureProject) {
  const QString path = ProjectsDir() + QStringLiteral("/general/many-phrases");
  ASSERT_TRUE(window_.OpenProject(path));
  EXPECT_EQ(window_.Model().PhraseCount(), 6);
  EXPECT_EQ(window_.windowTitle(),
            QStringLiteral("many-phrases[*] - Speak5220"));
  EXPECT_FALSE(window_.isWindowModified());
  EXPECT_THAT(window_.StatusText().toStdString(), HasSubstr("1 PHROM of 64"));
  EXPECT_EQ(window_.RecentPaths().front(), QDir(path).absolutePath());
  EXPECT_EQ(recent_.paths, window_.RecentPaths());
}

TEST_F(MainWindowProjectTest, ExportWritesTheFilesIntoTheChosenFolder) {
  const QString path = ProjectsDir() + QStringLiteral("/general/many-phrases");
  ASSERT_TRUE(window_.OpenProject(path));
  const QString out = Temp(QStringLiteral("out"));
  EXPECT_CALL(dialogs_, AskExport(_))
      .WillOnce([&](const ExportStart& start) -> std::optional<ExportRequest> {
        EXPECT_EQ(start.base, QStringLiteral("many-phrases"));
        EXPECT_EQ(
            start.directory,
            QDir(ProjectsDir() + QStringLiteral("/general")).absolutePath());
        return ExportRequest{
            .preset = {.name = "speech", .format = "c-header-phrom"},
            .directory = out};
      });
  window_.ExportProject();
  QCoreApplication::processEvents();
  EXPECT_EQ(window_.ExportsRunning(), 0);
  std::ifstream header(ToPath(out + QStringLiteral("/speech.h")));
  ASSERT_TRUE(header.good());
  std::string first;
  std::getline(header, first);
  EXPECT_EQ(first, "/*");
  EXPECT_FALSE(window_.isWindowModified());
}

TEST_F(MainWindowProjectTest, OpensAnUnpackedProjectThroughItsProjectYaml) {
  const QString path = ProjectsDir() + QStringLiteral("/general/one-phrase");
  ASSERT_TRUE(window_.OpenProject(path + QStringLiteral("/project.yaml")));
  EXPECT_EQ(window_.Document().Path(), QDir(path).absolutePath());
  EXPECT_EQ(window_.Model().PhraseCount(), 1);
}

TEST_F(MainWindowProjectTest, FailedOpenIsReportedAndKeepsTheProject) {
  const QString missing = Temp(QStringLiteral("missing.s5proj"));
  recent_.paths = {missing};
  MainWindow window(dialogs_, recent_, preferences_, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Open Project"), _))
      .WillOnce([](const QString&, const QString& message) {
        EXPECT_TRUE(message.startsWith(QStringLiteral("cannot open")));
      });
  EXPECT_FALSE(window.OpenProject(missing));
  EXPECT_EQ(window.Document().Path(), QString());
  // A recent file that no longer exists leaves the list.
  EXPECT_TRUE(window.RecentPaths().isEmpty());
}

TEST_F(MainWindowProjectTest, SaveAsWritesAProjectFile) {
  ASSERT_TRUE(window_.Document()
                  .Edits()
                  .InsertPhrases({{.index = 0, .phrase = AudioPhrase(1, "HI")}})
                  .IsOk());
  ASSERT_TRUE(window_.isWindowModified());
  EXPECT_CALL(dialogs_, SaveProjectPath(_))
      .WillOnce(Return(Temp(QStringLiteral("greetings"))));
  ASSERT_TRUE(window_.SaveAs());

  const QString saved = Temp(QStringLiteral("greetings.s5proj"));
  EXPECT_TRUE(fs::is_regular_file(ToPath(saved)));
  EXPECT_EQ(window_.windowTitle(),
            QStringLiteral("greetings.s5proj[*] - Speak5220"));
  EXPECT_FALSE(window_.isWindowModified());
  EXPECT_EQ(window_.RecentPaths().front(), saved);

  ProjectDocument reopened;
  ASSERT_TRUE(reopened.Open(saved).IsOk());
  EXPECT_EQ(reopened.Project().Phrases(),
            window_.Document().Project().Phrases());
  EXPECT_EQ(reopened.Project().Settings().name, "greetings");
}

TEST_F(MainWindowProjectTest, SaveWritesBackInTheSameForm) {
  const QString path = CopyFixture(QStringLiteral("general/one-phrase"));
  ASSERT_TRUE(window_.OpenProject(path));
  const core::project::PhraseId id =
      window_.Document().Project().Phrases().front().id;
  ASSERT_TRUE(
      window_.Document().Edits().RenamePhrase(id, "HELLO THERE").IsOk());
  ASSERT_TRUE(window_.Save());
  EXPECT_FALSE(window_.isWindowModified());
  EXPECT_TRUE(fs::is_directory(ToPath(path)));

  ProjectDocument reopened;
  ASSERT_TRUE(reopened.Open(path).IsOk());
  EXPECT_EQ(reopened.Project().Phrases().front().name, "HELLO THERE");
}

// Phrases changed since the last save are marked in the list and can be
// put back as saved, in one undoable step; saving clears the marks.
TEST_F(MainWindowProjectTest, ChangedPhrasesAreMarkedAndCanBeReverted) {
  const QString path = CopyFixture(QStringLiteral("general/many-phrases"));
  ASSERT_TRUE(window_.OpenProject(path));
  const core::project::Project saved = window_.Document().Project();
  const std::vector<core::project::PhraseId> ids = saved.Order();
  const auto marked = [this](int row) {
    return window_.Model()
        .index(row)
        .data(PhraseListModel::kModifiedRole)
        .toBool();
  };
  QAction* revert = nullptr;
  for (QAction* action : window_.findChildren<QAction*>()) {
    if (action->text() == QStringLiteral("Re&vert to Saved")) {
      revert = action;
    }
  }
  ASSERT_NE(revert, nullptr);
  window_.Panel()->Select({ids[0], ids[1]});
  EXPECT_FALSE(marked(0));
  EXPECT_FALSE(revert->isEnabled());

  // The first two phrases swap names; the third gets a description.
  ProjectEdits& edits = window_.Document().Edits();
  const std::string first = saved.Phrases()[0].name;
  const std::string second = saved.Phrases()[1].name;
  ASSERT_TRUE(edits.RenamePhrase(ids[0], "SPARE").IsOk());
  ASSERT_TRUE(edits.RenamePhrase(ids[1], first).IsOk());
  ASSERT_TRUE(edits.RenamePhrase(ids[0], second).IsOk());
  ASSERT_TRUE(edits.SetDescription(ids[2], "changed").IsOk());
  EXPECT_TRUE(marked(0));
  EXPECT_TRUE(marked(1));
  EXPECT_TRUE(marked(2));
  EXPECT_FALSE(marked(3));
  EXPECT_TRUE(revert->isEnabled());

  revert->trigger();
  EXPECT_EQ(window_.Document().Project().Phrases()[0], saved.Phrases()[0]);
  EXPECT_EQ(window_.Document().Project().Phrases()[1], saved.Phrases()[1]);
  EXPECT_EQ(window_.Document().UndoStack().undoText(),
            QStringLiteral("Revert 2 Phrases to Saved"));
  EXPECT_FALSE(marked(0));
  EXPECT_TRUE(marked(2));
  EXPECT_FALSE(revert->isEnabled());
  EXPECT_EQ(window_.Panel()->SelectedIds(),
            (std::vector<core::project::PhraseId>{ids[0], ids[1]}));
  window_.Document().UndoStack().undo();
  EXPECT_EQ(window_.Document().Project().Phrases()[0].name, second);
  EXPECT_TRUE(marked(0));

  ASSERT_TRUE(window_.Save());
  EXPECT_FALSE(marked(0));
  EXPECT_FALSE(marked(2));
  EXPECT_FALSE(revert->isEnabled());
}

TEST_F(MainWindowProjectTest, ClosingWithUnsavedChangesSavesWhenAsked) {
  const QString path = CopyFixture(QStringLiteral("general/one-phrase"));
  ASSERT_TRUE(window_.OpenProject(path));
  const core::project::PhraseId id =
      window_.Document().Project().Phrases().front().id;
  ASSERT_TRUE(window_.Document().Edits().SetDescription(id, "edited").IsOk());
  EXPECT_CALL(dialogs_, AskSaveChanges(QStringLiteral("one-phrase")))
      .WillOnce(Return(SaveChoice::kSave));
  EXPECT_TRUE(window_.close());

  ProjectDocument reopened;
  ASSERT_TRUE(reopened.Open(path).IsOk());
  EXPECT_EQ(reopened.Project().Phrases().front().description, "edited");
}

TEST_F(MainWindowProjectTest, OpeningAnotherProjectAsksToSaveFirst) {
  ASSERT_TRUE(window_.Document()
                  .Edits()
                  .InsertPhrases({{.index = 0, .phrase = AudioPhrase(1, "HI")}})
                  .IsOk());
  EXPECT_CALL(dialogs_, AskSaveChanges(_))
      .WillOnce(Return(SaveChoice::kCancel))
      .WillOnce(Return(SaveChoice::kDiscard));
  const QString path = ProjectsDir() + QStringLiteral("/general/one-phrase");
  EXPECT_FALSE(window_.OpenProject(path));
  EXPECT_EQ(window_.Model().PhraseCount(), 1);
  EXPECT_TRUE(window_.OpenProject(path));
  EXPECT_FALSE(window_.Document().UndoStack().canUndo());
}

TEST_F(MainWindowProjectTest, ImportsDroppedFilesAsOneStep) {
  WriteTone(Temp(QStringLiteral("good_morning.wav")), 16000, 0.3F);
  WriteTone(Temp(QStringLiteral("good_night.wav")), 8000, 0.3F);
  DropFiles(window_,
            {Temp(QStringLiteral("good_morning.wav")),
             Temp(QStringLiteral("good_night.wav"))},
            0);
  WaitForImports(window_);
  ASSERT_EQ(window_.Model().PhraseCount(), 2);
  EXPECT_EQ(window_.Model().index(0).data().toString(),
            QStringLiteral("GOOD MORNING"));
  EXPECT_EQ(window_.Model().index(1).data().toString(),
            QStringLiteral("GOOD NIGHT"));
  EXPECT_EQ(window_.Panel()->SelectedRows(), (QList<int>{0, 1}));
  EXPECT_TRUE(window_.isWindowModified());
  EXPECT_EQ(window_.Document().UndoStack().count(), 1);
  window_.Document().UndoStack().undo();
  EXPECT_EQ(window_.Model().PhraseCount(), 0);
}

TEST_F(MainWindowProjectTest, ClippingFileIsNormalisedWhenTheUserAgrees) {
  WriteTone(Temp(QStringLiteral("loud.wav")), 8000, 1.0F);
  EXPECT_CALL(dialogs_, AskNormalise(QStringList{QStringLiteral("loud.wav")}))
      .WillOnce(Return(true));
  window_.ImportFiles({Temp(QStringLiteral("loud.wav"))}, 0);
  WaitForImports(window_);
  ASSERT_EQ(window_.Document().Project().Phrases().size(), 1U);
  EXPECT_NEAR(Peak(window_.Document().Project().Phrases().front().audio),
              0.891F, 0.001F);
}

TEST_F(MainWindowProjectTest, ClippingFileIsKeptWhenTheUserDeclines) {
  WriteTone(Temp(QStringLiteral("loud.wav")), 8000, 1.0F);
  EXPECT_CALL(dialogs_, AskNormalise(_)).WillOnce(Return(false));
  window_.ImportFiles({Temp(QStringLiteral("loud.wav"))}, 0);
  WaitForImports(window_);
  ASSERT_EQ(window_.Document().Project().Phrases().size(), 1U);
  EXPECT_GT(Peak(window_.Document().Project().Phrases().front().audio), 0.999F);
}

TEST_F(MainWindowProjectTest, FilesThatCannotBeImportedAreReported) {
  WriteTone(Temp(QStringLiteral("fine.wav")), 8000, 0.3F);
  const QString broken = Temp(QStringLiteral("notes.txt"));
  {
    std::ofstream file(ToPath(broken));
    file << "not audio";
  }
  EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Add Phrases"), _))
      .WillOnce([&broken](const QString&, const QString& message) {
        EXPECT_TRUE(message.contains(broken));
      });
  window_.ImportFiles({Temp(QStringLiteral("fine.wav")), broken}, 0);
  WaitForImports(window_);
  EXPECT_EQ(window_.Model().PhraseCount(), 1);
}

TEST(MainWindowThreadedImportTest, ImportsOnAThreadPool) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const QString path = directory.filePath(QStringLiteral("hello.wav"));
  WriteTone(path, 44100, 0.3F);
  StrictMock<MockDialogs> dialogs;
  FakeRecentFilesStore recent;
  FakeViewPreferences preferences;
  core::ThreadPoolTaskRunner runner(2);
  TestPlayback playback;
  testing::TestEncoding encoding;
  MainWindow window(dialogs, recent, preferences, runner, playback.synth,
                    playback.player, encoding.Tools());
  window.ImportFiles({path}, 0);
  EXPECT_EQ(window.ImportsRunning(), 1);
  WaitForImports(window);
  ASSERT_EQ(window.Model().PhraseCount(), 1);
  EXPECT_EQ(window.Model().index(0).data().toString(), QStringLiteral("HELLO"));
}

}  // namespace
}  // namespace speak5220::app
