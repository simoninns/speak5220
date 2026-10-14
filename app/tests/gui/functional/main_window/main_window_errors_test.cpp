/*
 * File:        main_window_errors_test.cpp
 * Module:      main_window tests
 * Purpose:     Functional tests of how the main window reports corrupt WAV
 *              files, corrupt projects and audio device failures: a clear
 *              message, and no crash
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QCoreApplication>
#include <QDir>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QString>
#include <QStringList>
#include <QTemporaryDir>
#include <QToolButton>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <string>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/audio/wav_io.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/main_window.h"
#include "gui/main_window/project_document.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/playback/player.h"
#include "gui/support/gui_mocks.h"
#include "gui/waveform/phrase_waveforms.h"

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

// How long an import may take before a test gives up.
constexpr qint64 kImportTimeoutMs = 20000;

QString ProjectsDir() { return QStringLiteral(SPEAK5220_PROJECTS_DIR); }

fs::path ToPath(const QString& text) { return {text.toStdU16String()}; }

void WriteBytes(const QString& path, const std::string& bytes) {
  std::ofstream file(ToPath(path), std::ios::binary);
  file << bytes;
}

std::string ReadBytes(const QString& path) {
  std::ifstream file(ToPath(path), std::ios::binary);
  return {std::istreambuf_iterator<char>(file),
          std::istreambuf_iterator<char>()};
}

// A one-second 220 Hz tone at 8 kHz.
void WriteTone(const QString& path) {
  constexpr int kRate = 8000;
  constexpr double kFrequency = 220.0;
  std::vector<float> samples(kRate);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] =
        0.3F * static_cast<float>(std::sin(2.0 * std::numbers::pi * kFrequency *
                                           static_cast<double>(i) / kRate));
  }
  core::audio::SndfileWavWriter writer;
  ASSERT_TRUE(writer
                  .Write(ToPath(path), samples, kRate,
                         core::audio::WavOutputFormat::kPcm16)
                  .IsOk());
}

void WaitForImports(MainWindow& window) {
  QElapsedTimer timer;
  timer.start();
  QCoreApplication::processEvents();
  while (window.ImportsRunning() > 0 && timer.elapsed() < kImportTimeoutMs) {
    QCoreApplication::processEvents(QEventLoop::AllEvents, 10);
  }
  ASSERT_EQ(window.ImportsRunning(), 0) << "the import did not finish";
}

class MainWindowErrorsTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(directory_.isValid()); }

  QString Temp(const QString& name) const { return directory_.filePath(name); }

  // A copy of the fixture project general/one-phrase, unpacked.
  QString CopyOnePhrase() {
    const QString target = Temp(QStringLiteral("one-phrase"));
    fs::copy(ToPath(ProjectsDir() + QStringLiteral("/general/one-phrase")),
             ToPath(target), fs::copy_options::recursive);
    return target;
  }

  // The path of the copy's one phrase file `name`.
  static QString PhraseFile(const QString& project, const QString& name) {
    const QDir phrases(project + QStringLiteral("/phrases"));
    const QStringList ids =
        phrases.entryList(QDir::Dirs | QDir::NoDotAndDotDot);
    return phrases.filePath(ids.front() + QLatin1Char('/') + name);
  }

  // Imports `path` and returns the message reported for it; checks that
  // nothing was added.
  QString ImportFailure(const QString& path) {
    QString reported;
    EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Add Phrases"), _))
        .WillOnce([&reported](const QString&, const QString& message) {
          reported = message;
        });
    window_.ImportFiles({path}, 0);
    WaitForImports(window_);
    EXPECT_EQ(window_.Model().PhraseCount(), 0);
    ::testing::Mock::VerifyAndClearExpectations(&dialogs_);
    return reported;
  }

  // Opens `path` and returns the message reported for it; checks that the
  // project open before stays open.
  QString OpenFailure(const QString& path) {
    QString reported;
    EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Open Project"), _))
        .WillOnce([&reported](const QString&, const QString& message) {
          reported = message;
        });
    EXPECT_FALSE(window_.OpenProject(path));
    EXPECT_EQ(window_.Document().Path(), QString());
    EXPECT_EQ(window_.Model().PhraseCount(), 0);
    ::testing::Mock::VerifyAndClearExpectations(&dialogs_);
    return reported;
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

// Corrupt WAV files: each is reported with its path and why it cannot be
// read.

TEST_F(MainWindowErrorsTest, EmptyWavFileIsReported) {
  const QString path = Temp(QStringLiteral("empty.wav"));
  WriteBytes(path, "");
  const QString message = ImportFailure(path);
  EXPECT_TRUE(message.contains(path)) << message.toStdString();
  EXPECT_THAT(message.toStdString(), HasSubstr("as a WAV file"));
}

TEST_F(MainWindowErrorsTest, WavFileWithOnlyItsHeaderIsReported) {
  const QString path = Temp(QStringLiteral("header.wav"));
  WriteBytes(path, std::string("RIFF\x04\x00\x00\x00WAVE", 12));
  const QString message = ImportFailure(path);
  EXPECT_TRUE(message.contains(path)) << message.toStdString();
  EXPECT_THAT(message.toStdString(), HasSubstr("as a WAV file"));
}

TEST_F(MainWindowErrorsTest, WavFileOfAnUnknownEncodingIsReported) {
  // A complete header whose format tag (0x1234) is no encoding libsndfile
  // knows.
  const std::string header(
      "RIFF\x28\x00\x00\x00WAVEfmt \x10\x00\x00\x00"
      "\x34\x12\x01\x00\x40\x1f\x00\x00\x80\x3e\x00\x00\x02\x00\x10\x00"
      "data\x04\x00\x00\x00\x00\x00\x00\x00",
      48);
  const QString path = Temp(QStringLiteral("codec.wav"));
  WriteBytes(path, header);
  const QString message = ImportFailure(path);
  EXPECT_TRUE(message.contains(path)) << message.toStdString();
  EXPECT_THAT(message.toStdString(), HasSubstr("as a WAV file"));
}

TEST_F(MainWindowErrorsTest, TextFileNamedWavIsReported) {
  const QString path = Temp(QStringLiteral("notes.wav"));
  WriteBytes(path, "These are not samples.\n");
  const QString message = ImportFailure(path);
  EXPECT_TRUE(message.contains(path)) << message.toStdString();
}

TEST_F(MainWindowErrorsTest, MissingWavFileIsReported) {
  const QString path = Temp(QStringLiteral("gone.wav"));
  const QString message = ImportFailure(path);
  EXPECT_TRUE(message.contains(path)) << message.toStdString();
}

TEST_F(MainWindowErrorsTest, TruncatedWavFileImportsWhatIsThereOrIsReported) {
  // The data chunk claims a second of samples; the file stops after a few.
  const QString whole = Temp(QStringLiteral("whole.wav"));
  WriteTone(whole);
  const QString path = Temp(QStringLiteral("cut.wav"));
  WriteBytes(path, ReadBytes(whole).substr(0, 200));
  EXPECT_CALL(dialogs_, ShowError(_, _))
      .Times(::testing::AtMost(1))
      .WillRepeatedly([&path](const QString&, const QString& message) {
        EXPECT_TRUE(message.contains(path)) << message.toStdString();
      });
  window_.ImportFiles({path}, 0);
  WaitForImports(window_);
  EXPECT_LE(window_.Model().PhraseCount(), 1);
}

// Corrupt projects: each is reported as "cannot open PATH: why", and the
// project already open stays open.

TEST_F(MainWindowErrorsTest, ProjectFileThatIsNotAZipIsReported) {
  const QString path = Temp(QStringLiteral("garbage.s5proj"));
  WriteBytes(path, std::string(4096, '\x5A'));
  const QString message = OpenFailure(path);
  EXPECT_TRUE(message.startsWith(QStringLiteral("cannot open")))
      << message.toStdString();
  EXPECT_THAT(message.toStdString(), HasSubstr("garbage.s5proj"));
}

TEST_F(MainWindowErrorsTest, TruncatedProjectFileIsReported) {
  // A project saved whole, then cut in half.
  ASSERT_TRUE(window_.OpenProject(CopyOnePhrase()));
  const QString saved = Temp(QStringLiteral("saved.s5proj"));
  EXPECT_CALL(dialogs_, SaveProjectPath(_)).WillOnce(Return(saved));
  ASSERT_TRUE(window_.SaveAs());
  ASSERT_TRUE(window_.NewProject());
  const std::string bytes = ReadBytes(saved);
  const QString path = Temp(QStringLiteral("half.s5proj"));
  WriteBytes(path, bytes.substr(0, bytes.size() / 2));
  const QString message = OpenFailure(path);
  EXPECT_TRUE(message.startsWith(QStringLiteral("cannot open")))
      << message.toStdString();
}

TEST_F(MainWindowErrorsTest, ProjectYamlThatIsNotYamlIsReported) {
  const QString project = CopyOnePhrase();
  WriteBytes(project + QStringLiteral("/project.yaml"), "phrases: [: : {\n");
  const QString message = OpenFailure(project);
  EXPECT_TRUE(message.startsWith(QStringLiteral("cannot open")))
      << message.toStdString();
  EXPECT_THAT(message.toStdString(), HasSubstr("project.yaml"));
}

TEST_F(MainWindowErrorsTest, ProjectYamlWithAWrongTypeIsReported) {
  const QString project = CopyOnePhrase();
  std::string yaml = ReadBytes(project + QStringLiteral("/project.yaml"));
  const std::string from = "target_rms: 20";
  ASSERT_NE(yaml.find(from), std::string::npos);
  yaml.replace(yaml.find(from), from.size(), "target_rms: loud");
  WriteBytes(project + QStringLiteral("/project.yaml"), yaml);
  const QString message = OpenFailure(project);
  EXPECT_THAT(message.toStdString(), HasSubstr("target_rms"));
}

TEST_F(MainWindowErrorsTest, ProjectWithACorruptPhraseWavIsReported) {
  const QString project = CopyOnePhrase();
  WriteBytes(PhraseFile(project, QStringLiteral("source.wav")), "RIFF");
  const QString message = OpenFailure(project);
  EXPECT_TRUE(message.startsWith(QStringLiteral("cannot open")))
      << message.toStdString();
  EXPECT_THAT(message.toStdString(), HasSubstr("source.wav"));
}

TEST_F(MainWindowErrorsTest, ProjectWithCorruptFramesIsReported) {
  const QString project = CopyOnePhrase();
  WriteBytes(PhraseFile(project, QStringLiteral("frames.yaml")),
             "frames: [[1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14]]\n");
  const QString message = OpenFailure(project);
  EXPECT_TRUE(message.startsWith(QStringLiteral("cannot open")))
      << message.toStdString();
}

TEST_F(MainWindowErrorsTest, ProjectWithAMissingPhraseIsReported) {
  const QString project = CopyOnePhrase();
  fs::remove_all(
      ToPath(QFileInfo(PhraseFile(project, QStringLiteral("source.wav")))
                 .absolutePath()));
  const QString message = OpenFailure(project);
  EXPECT_TRUE(message.startsWith(QStringLiteral("cannot open")))
      << message.toStdString();
}

// Audio device failures: the device cannot be opened, or fails while
// playing.

TEST_F(MainWindowErrorsTest, DeviceThatCannotPlayIsReported) {
  ASSERT_TRUE(window_.OpenProject(CopyOnePhrase()));
  window_.Panel()->Select({window_.Model().IdAt(0)});
  ON_CALL(playback_.output, Start(_, _))
      .WillByDefault(Return(core::UnavailableError(
          "there is no audio output device: connect one, or check the "
          "system's sound settings")));
  EXPECT_CALL(dialogs_,
              ShowError(QStringLiteral("Play"),
                        QStringLiteral("there is no audio output device: "
                                       "connect one, or check the system's "
                                       "sound settings")));
  window_.Waveforms()->PlayButton(PlaybackSource::kOriginal)->click();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kStopped);
}

TEST_F(MainWindowErrorsTest, DeviceFailingWhilePlayingIsReported) {
  ASSERT_TRUE(window_.OpenProject(CopyOnePhrase()));
  window_.Panel()->Select({window_.Model().IdAt(0)});
  window_.Waveforms()->PlayButton(PlaybackSource::kOriginal)->click();
  ASSERT_EQ(playback_.player.State(), PlaybackState::kPlaying);
  ON_CALL(playback_.output, IsFinished()).WillByDefault(Return(true));
  ON_CALL(playback_.output, DeviceStatus())
      .WillByDefault(Return(core::UnavailableError(
          "the audio device Speakers stopped while playing")));
  EXPECT_CALL(dialogs_,
              ShowError(QStringLiteral("Play"),
                        QStringLiteral("the audio device Speakers stopped "
                                       "while playing")));
  playback_.player.Tick();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kStopped);
  // The window goes on working: playing again tries the device again.
  ON_CALL(playback_.output, IsFinished()).WillByDefault(Return(false));
  ON_CALL(playback_.output, DeviceStatus())
      .WillByDefault(Return(core::OkStatus()));
  window_.Waveforms()->PlayButton(PlaybackSource::kOriginal)->click();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
}

}  // namespace
}  // namespace speak5220::app
