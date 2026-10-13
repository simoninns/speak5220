/*
 * File:        main.cpp
 * Module:      gui
 * Purpose:     Entry point of the speak5220 GUI executable
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <memory>
#include <string_view>

#include "core/dsp/resampler.h"
#include "core/encoder/encoder.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/synth.h"
#include "core/util/clock.h"
#include "core/util/logging.h"
#include "core/util/logging_options.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "core/util/version.h"
#include "gui/encoder_panel/phrase_encoder.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/main_window.h"
#include "gui/main_window/recent_files.h"
#include "gui/main_window/view_preferences.h"
#include "gui/playback/player.h"
#include "gui/playback/qt_audio_output.h"
#include "gui/theme/colour_scheme.h"

namespace {

// Exit code for a command line that cannot be used.
constexpr int kUsageExitCode = 2;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

int main(int argc, char* argv[]) {
  namespace core = speak5220::core;
  QApplication application(argc, argv);
  QApplication::setApplicationName(ToQString(core::kApplicationName));
  // Where QSettings keeps the preferences (the recent-file list, the
  // advanced encoder settings shown or not, the theme).
  QApplication::setOrganizationName(ToQString(core::kApplicationName));
  QApplication::setApplicationVersion(ToQString(core::Version()));
  // The icon of every window, and of the Dock on macOS. On Wayland the
  // desktop takes it from the .desktop file named here instead.
  QApplication::setWindowIcon(
      QIcon(QStringLiteral(":/speak5220/speak5220-icon.png")));
  QApplication::setDesktopFileName(QStringLiteral(SPEAK5220_APP_ID));

  QCommandLineParser parser;
  parser.addHelpOption();
  parser.addVersionOption();
  core::AddLoggingOptions(parser);
  parser.addPositionalArgument(
      QStringLiteral("project"),
      QStringLiteral("A project to open: a .s5proj file or a project "
                     "directory."),
      QStringLiteral("[project]"));
  parser.process(application);

  const core::StatusOr<core::LogSettings> settings =
      core::LogSettingsFromParser(parser);
  if (!settings.IsOk()) {
    QTextStream(stderr) << "speak5220: "
                        << ToQString(settings.GetStatus().Message()) << '\n';
    return kUsageExitCode;
  }
  if (const core::Status status = core::ConfigureLogging(*settings);
      !status.IsOk()) {
    QTextStream(stderr) << "speak5220: " << ToQString(status.Message()) << '\n';
    return kUsageExitCode;
  }
  core::Logger(core::kLogModuleGui)
      ->info("speak5220 {} started", core::Version());

  const QStringList projects = parser.positionalArguments();
  if (projects.size() > 1) {
    QTextStream(stderr) << "speak5220: give at most one project\n";
    return kUsageExitCode;
  }

  speak5220::app::QtDialogs dialogs;
  speak5220::app::SettingsRecentFilesStore recent_files;
  speak5220::app::SettingsViewPreferences view_preferences;

  // Plan §5, "Colours": light or dark as the desktop prefers, unless the
  // user chose one (View > Theme). Applied before the window is shown.
  const std::unique_ptr<speak5220::app::IDesktopColourScheme> desktop_scheme =
      speak5220::app::MakeDesktopColourScheme();
  speak5220::app::ColourSchemeFollower follower(*desktop_scheme,
                                                view_preferences.ChosenTheme());
  core::ThreadPoolTaskRunner runner;
  core::synth::Synth synth;
  speak5220::app::QtAudioOutput audio_output;
  core::SteadyClock clock;
  core::dsp::SincResampler resampler;
  speak5220::app::Player player(audio_output, clock, resampler);
  // Plan §3.3: the quality measures for the encoder panel and Optimise.
  // Their synth keeps nothing, since Optimise speaks hundreds of
  // candidates once each.
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  core::synth::Synth quality_synth({}, core::synth::SynthCache::kOff);
  core::quality::QualityAssessor quality(quality_synth);
  // One search at a time; its candidates run on `runner`.
  core::ThreadPoolTaskRunner optimiser_driver(1);
  const speak5220::app::EncodingTools tools{
      .encoder = encoder.get(),
      .quality = &quality,
      .clock = &clock,
      .optimiser_driver = &optimiser_driver};
  speak5220::app::MainWindow window(dialogs, recent_files, view_preferences,
                                    runner, synth, player, tools);
  dialogs.SetParent(&window);
  QObject::connect(&window, &speak5220::app::MainWindow::ThemeChanged, &window,
                   [&follower](speak5220::app::ThemeChoice choice) {
                     follower.SetChoice(choice);
                   });
  window.show();
  if (!projects.isEmpty()) {
    window.OpenProject(projects.front());
  }
  const int result = QApplication::exec();
  core::Logger(core::kLogModuleGui)->info("speak5220 finished");
  core::ShutdownLogging();
  return result;
}
