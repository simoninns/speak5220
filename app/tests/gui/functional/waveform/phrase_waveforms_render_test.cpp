/*
 * File:        phrase_waveforms_render_test.cpp
 * Module:      waveform tests
 * Purpose:     Functional test of the waveform panes: a fixture phrase
 *              spoken by the emulator, lined up and drawn offscreen
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QApplication>
#include <QImage>
#include <QPixmap>
#include <QString>
#include <cstdlib>
#include <vector>

#include "core/project/phrase.h"
#include "core/synth/synth.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/main_window/main_window.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/support/gui_mocks.h"
#include "gui/waveform/phrase_waveforms.h"
#include "gui/waveform/waveform_pane.h"
#include "gui/waveform/waveform_view.h"

namespace speak5220::app {
namespace {

using testing::FakeRecentFilesStore;
using testing::FakeViewPreferences;
using testing::MockDialogs;
using ::testing::NiceMock;
using testing::TestPlayback;

QString ProjectsDir() { return QStringLiteral(SPEAK5220_PROJECTS_DIR); }

TEST(PhraseWaveformsRenderTest, DrawsAFixturePhraseLinedUpWithItsSource) {
  NiceMock<MockDialogs> dialogs;
  FakeRecentFilesStore recent;
  FakeViewPreferences preferences;
  core::InlineTaskRunner runner;
  core::synth::Synth synth;
  TestPlayback playback;
  testing::TestEncoding encoding;
  MainWindow window(dialogs, recent, preferences, runner, synth,
                    playback.player, encoding.Tools());
  window.resize(1200, 700);
  window.show();
  ASSERT_TRUE(window.OpenProject(ProjectsDir() +
                                 QStringLiteral("/general/many-phrases")));
  const core::project::Phrase& phrase =
      window.Document().Project().Phrases().front();
  ASSERT_TRUE(phrase.encoding.has_value());
  window.Panel()->Select({phrase.id});
  QApplication::processEvents();

  const PhraseWaveforms* waveforms = window.Waveforms();
  ASSERT_EQ(waveforms->RendersRunning(), 0);
  const WaveformView* original = waveforms->OriginalPane()->View();
  const WaveformView* encoded = waveforms->EncodedPane()->View();
  ASSERT_TRUE(original->HasSamples());
  ASSERT_TRUE(encoded->HasSamples()) << encoded->Placeholder().toStdString();
  // The emulator starts speaking about 25 ms in, and the encoder leaves out
  // leading silence: source sample 0 is within a second of the start.
  EXPECT_GT(encoded->SourceOffset(), -8000);
  EXPECT_LT(encoded->SourceOffset(), 8000);
  EXPECT_EQ(encoded->Axis(), original->Axis());

  const QImage image = window.grab().toImage();
  ASSERT_FALSE(image.isNull());
  // The encoded pane is drawn: it is not all one colour.
  const QImage pane = waveforms->EncodedPane()->View()->grab().toImage();
  bool varied = false;
  for (int x = 0; x < pane.width() && !varied; ++x) {
    varied = pane.pixel(x, pane.height() / 2) != pane.pixel(0, 0);
  }
  EXPECT_TRUE(varied);
}

}  // namespace
}  // namespace speak5220::app
