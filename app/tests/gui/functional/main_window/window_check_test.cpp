/*
 * File:        window_check_test.cpp
 * Module:      main_window tests
 * Purpose:     The by-hand check of every window and dialog in light and
 *              dark, at normal and high pixel density: renders each to a
 *              PNG file to be looked at
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>
#include <QColor>
#include <QDir>
#include <QPalette>
#include <QPixmap>
#include <QString>
#include <QTabWidget>
#include <QWidget>
#include <array>
#include <cstdlib>

#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/about/about_dialog.h"
#include "gui/encoder_panel/encoder_panel.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/export/export_dialog.h"
#include "gui/main_window/main_window.h"
#include "gui/optimise/optimise_dialog.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/preferences/preferences_dialog.h"
#include "gui/project_settings/project_settings_dialog.h"
#include "gui/support/gui_mocks.h"
#include "gui/waveform/phrase_tab.h"
#include "gui/waveform/phrase_waveforms.h"

namespace speak5220::app {
namespace {

using testing::FakeRecentFilesStore;
using testing::FakeViewPreferences;
using testing::MockDialogs;
using ::testing::NiceMock;
using testing::TestPlayback;

// The pixel densities checked: a normal screen and a high-density one.
constexpr std::array<int, 2> kPixelRatios = {1, 2};

// A dark palette with every role filled, as Fusion's dark scheme gives.
QPalette DarkPalette() {
  QPalette palette(QColor(0x3C, 0x3C, 0x3C), QColor(0x32, 0x32, 0x32));
  for (const QPalette::ColorRole role :
       {QPalette::WindowText, QPalette::Text, QPalette::ButtonText,
        QPalette::BrightText}) {
    palette.setColor(role, QColor(0xFF, 0xFF, 0xFF));
  }
  palette.setColor(QPalette::Base, QColor(0x1E, 0x1E, 0x1E));
  palette.setColor(QPalette::AlternateBase, QColor(0x2A, 0x2A, 0x2A));
  palette.setColor(QPalette::PlaceholderText, QColor(0x9A, 0x9A, 0x9A));
  palette.setColor(QPalette::Highlight, QColor(0x30, 0x8C, 0xC6));
  palette.setColor(QPalette::HighlightedText, QColor(0xFF, 0xFF, 0xFF));
  palette.setColor(QPalette::Link, QColor(0x8A, 0xB4, 0xF8));
  palette.setColor(QPalette::ToolTipBase, QColor(0x3C, 0x3C, 0x3C));
  palette.setColor(QPalette::ToolTipText, QColor(0xFF, 0xFF, 0xFF));
  palette.setColor(QPalette::Disabled, QPalette::Text,
                   QColor(0x80, 0x80, 0x80));
  palette.setColor(QPalette::Disabled, QPalette::WindowText,
                   QColor(0x80, 0x80, 0x80));
  palette.setColor(QPalette::Disabled, QPalette::ButtonText,
                   QColor(0x80, 0x80, 0x80));
  return palette;
}

// Renders `widget` to OUT/NAME-SCHEME-Nx.png for each pixel ratio.
void Render(QWidget& widget, const QString& out, const QString& name,
            const QString& scheme) {
  QApplication::processEvents();
  for (const int ratio : kPixelRatios) {
    QPixmap pixmap(widget.size() * ratio);
    pixmap.setDevicePixelRatio(ratio);
    widget.render(&pixmap);
    const QString path = QDir(out).filePath(
        QStringLiteral("%1-%2-%3x.png").arg(name, scheme).arg(ratio));
    EXPECT_TRUE(pixmap.save(path)) << path.toStdString();
  }
}

// Renders each tab of `tabs` in `widget`.
void RenderTabs(QWidget& widget, QTabWidget& tabs, const QString& out,
                const QString& name, const QString& scheme) {
  for (int tab = 0; tab < tabs.count(); ++tab) {
    tabs.setCurrentIndex(tab);
    Render(widget, out, QStringLiteral("%1-tab%2").arg(name).arg(tab + 1),
           scheme);
  }
}

// By hand only (app/scripts/window-check.sh): the images go to the
// directory in SPEAK5220_WINDOW_CHECK_DIR, to be looked at for text that
// cannot be read, colours of the wrong scheme, and blurred or clipped
// drawing at 2x.
TEST(WindowCheck, DISABLED_RendersEveryWindowInLightAndDark) {
  const char* out_env = std::getenv("SPEAK5220_WINDOW_CHECK_DIR");
  ASSERT_NE(out_env, nullptr) << "set SPEAK5220_WINDOW_CHECK_DIR";
  const QString out = QString::fromLocal8Bit(out_env);
  ASSERT_TRUE(QDir().mkpath(out));
  const QPalette light = QApplication::palette();

  NiceMock<MockDialogs> dialogs;
  FakeRecentFilesStore recent;
  FakeViewPreferences preferences;
  core::InlineTaskRunner runner;
  TestPlayback playback;
  testing::TestEncoding encoding;
  MainWindow window(dialogs, recent, preferences, runner, playback.synth,
                    playback.player, encoding.Tools());
  ASSERT_TRUE(window.OpenProject(QStringLiteral(SPEAK5220_PROJECTS_DIR) +
                                 QStringLiteral("/general/many-phrases")));
  window.show();
  window.Panel()->Select({window.Model().IdAt(0)});

  for (const QString& scheme :
       {QStringLiteral("light"), QStringLiteral("dark")}) {
    QApplication::setPalette(scheme == QStringLiteral("dark") ? DarkPalette()
                                                              : light);
    for (const PhraseTab tab : kPhraseTabs) {
      window.Waveforms()->SetCurrentTab(tab);
      Render(window, out,
             QStringLiteral("main-%1").arg(static_cast<int>(tab) + 1), scheme);
    }
    window.Waveforms()->SetCurrentTab(PhraseTab::kEncode);
    window.Encoder()->Form()->SetAdvancedShown(true);
    Render(window, out, QStringLiteral("main-advanced"), scheme);
    window.Encoder()->Form()->SetAdvancedShown(false);

    window.Optimise({window.Model().IdAt(0)});
    window.OptimiseWindow()->resize(760, 560);
    Render(*window.OptimiseWindow(), out, QStringLiteral("optimise"), scheme);
    window.OptimiseWindow()->reject();

    ExportDialog export_dialog({.project = &window.Document().Project(),
                                .selected = {},
                                .base = QStringLiteral("speech"),
                                .directory = QDir::homePath(),
                                .confirm_overwrite = {},
                                .set_presets = [](const auto&, const QString&) {
                                  return core::OkStatus();
                                }});
    export_dialog.show();
    Render(export_dialog, out, QStringLiteral("export"), scheme);

    ProjectSettingsDialog settings_dialog(window.Document().Project());
    settings_dialog.resize(560, 520);
    settings_dialog.show();
    RenderTabs(settings_dialog, *settings_dialog.Tabs(), out,
               QStringLiteral("project-settings"), scheme);

    PreferencesDialog preferences_dialog(Preferences{}, {});
    preferences_dialog.resize(640, 620);
    preferences_dialog.show();
    RenderTabs(preferences_dialog, *preferences_dialog.Tabs(), out,
               QStringLiteral("preferences"), scheme);

    AboutDialog about;
    about.show();
    RenderTabs(about, *about.Tabs(), out, QStringLiteral("about"), scheme);
  }
  QApplication::setPalette(light);
}

}  // namespace
}  // namespace speak5220::app
