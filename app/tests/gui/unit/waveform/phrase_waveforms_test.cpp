/*
 * File:        phrase_waveforms_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of the phrase editor's tabs with a mocked
 *              editor, synth and audio output: rendering, linked time
 *              axes, the play-start marker and playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/phrase_waveforms.h"

#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QLabel>
#include <QList>
#include <QObject>
#include <QPalette>
#include <QRect>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QString>
#include <QTabBar>
#include <QToolButton>
#include <QUndoStack>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/output_filter.h"
#include "core/preprocess/preprocess.h"
#include "core/project/phrase.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/playback/player.h"
#include "gui/support/gui_mocks.h"
#include "gui/support/test_palettes.h"
#include "gui/undo/project_edits.h"
#include "gui/waveform/phrase_tab.h"
#include "gui/waveform/time_axis.h"
#include "gui/waveform/waveform_pane.h"
#include "gui/waveform/waveform_view.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"
#include "support/synthetic_speech.h"

namespace speak5220::app {
namespace {

using core::project::Phrase;
using core::quality::kFirstFrameStretch;
using ::testing::_;
using ::testing::DoAll;
using testing::EncodedPhrase;
using testing::MockProjectEditor;
using ::testing::Return;
using ::testing::SaveArg;
using ::testing::SizeIs;
using ::testing::StrictMock;
using testing::TestId;
using testing::TestPlayback;

// The phrase's audio: 0.2 s at 8 kHz; it plays from sample 400.
constexpr int kAudioSamples = 1600;
constexpr int kPlayStart = 400;
// The encoded audio's length in these tests.
constexpr int kEncodedSamples = 2000;
constexpr std::int64_t kMillisecond = 1000;

core::synth::SynthResult Idle(int samples) {
  core::synth::SynthResult result;
  result.codes.assign(static_cast<std::size_t>(samples), -1);
  result.samples.assign(static_cast<std::size_t>(samples), -64);
  result.leading_idle_samples = static_cast<std::size_t>(samples);
  return result;
}

class PhraseWaveformsTest : public ::testing::Test {
 protected:
  PhraseWaveformsTest() {
    phrase_.audio.assign(kAudioSamples, 0.1F);
    phrase_.play_start = kPlayStart;
    ON_CALL(playback_.synth, Synthesize(_, _))
        .WillByDefault(Return(Idle(kEncodedSamples)));
    waveforms_.resize(1000, 400);
    waveforms_.show();
  }

  // Shows the phrase and lets its render come back.
  void ShowPhrase() {
    waveforms_.ShowPhrase(&phrase_);
    QCoreApplication::processEvents();
  }

  // Shows `tab`, laid out at the widget's size.
  void ShowTab(PhraseTab tab) {
    waveforms_.SetCurrentTab(tab);
    QCoreApplication::processEvents();
  }

  WaveformView* Original() const { return waveforms_.OriginalPane()->View(); }
  WaveformView* Preprocessed() const {
    return waveforms_.PreprocessedPane()->View();
  }
  WaveformView* Encoded() const { return waveforms_.EncodedPane()->View(); }

  StrictMock<MockProjectEditor> editor_;
  QUndoStack stack_;
  ProjectEdits edits_{editor_, stack_};
  TestPlayback playback_;
  core::InlineTaskRunner runner_;
  PhraseWaveforms waveforms_{edits_, playback_.player, playback_.synth,
                             runner_};
  Phrase phrase_ = EncodedPhrase(1, "HELLO", 100);
};

TEST_F(PhraseWaveformsTest, RendersTheEncodedAudioOnceInTheBackground) {
  EXPECT_CALL(playback_.synth, Synthesize(_, core::synth::SynthPath::kVsm))
      .Times(1);
  ShowPhrase();
  EXPECT_EQ(waveforms_.RendersRunning(), 0);
  ASSERT_TRUE(Encoded()->HasSamples());
  // A silent output puts frame 0's stretch at source sample 0.
  EXPECT_EQ(Encoded()->SourceOffset(), kFirstFrameStretch);
  EXPECT_EQ(Encoded()->FrameAtX(Encoded()->XOfSample(kFirstFrameStretch)), 0);
  EXPECT_TRUE(Original()->HasSamples());

  // Showing it again, or after another phrase, uses the render kept.
  ShowPhrase();
  const Phrase other = EncodedPhrase(2, "OTHER", 50);
  EXPECT_CALL(playback_.synth, Synthesize(_, _))
      .WillOnce(Return(Idle(kEncodedSamples)))
      .RetiresOnSaturation();
  waveforms_.ShowPhrase(&other);
  QCoreApplication::processEvents();
  ShowPhrase();
  EXPECT_TRUE(Encoded()->HasSamples());
}

TEST_F(PhraseWaveformsTest, TabsAreTheOriginalThePreprocessedAndTheEncoded) {
  QTabBar* tabs = waveforms_.Tabs();
  ASSERT_EQ(tabs->count(), 3);
  EXPECT_EQ(tabs->tabText(0), QStringLiteral("Original"));
  EXPECT_EQ(tabs->tabText(1), QStringLiteral("Preprocess"));
  EXPECT_EQ(tabs->tabText(2), QStringLiteral("Encode"));
  EXPECT_EQ(waveforms_.OriginalPane()->Title(),
            QStringLiteral("Original (8 kHz)"));
  EXPECT_EQ(waveforms_.PreprocessedPane()->Title(),
            QStringLiteral("Preprocessed (8 kHz)"));
  EXPECT_EQ(waveforms_.EncodedPane()->Title(), QStringLiteral("Encoded"));
  EXPECT_EQ(waveforms_.CurrentTab(), PhraseTab::kOriginal);

  std::vector<PhraseTab> shown;
  QObject::connect(&waveforms_, &PhraseWaveforms::CurrentTabChanged,
                   [&shown](PhraseTab tab) { shown.push_back(tab); });
  waveforms_.SetCurrentTab(PhraseTab::kEncode);
  tabs->setCurrentIndex(1);
  EXPECT_EQ(shown, (std::vector<PhraseTab>{PhraseTab::kEncode,
                                           PhraseTab::kPreprocess}));
  EXPECT_EQ(waveforms_.CurrentTab(), PhraseTab::kPreprocess);
}

// A tab's settings go in its settings area, under the waveform area, with
// one splitter between them.
TEST_F(PhraseWaveformsTest, TabSettingsGoInTheSettingsArea) {
  auto* settings = new QLabel(QStringLiteral("settings"));
  waveforms_.SetControls(PhraseTab::kPreprocess, settings);
  EXPECT_EQ(waveforms_.SettingsArea(PhraseTab::kPreprocess)->widget(),
            settings);
  QSplitter* splitter = waveforms_.Splitter();
  ASSERT_EQ(splitter->count(), 2);
  EXPECT_EQ(splitter->orientation(), Qt::Vertical);
  EXPECT_FALSE(splitter->childrenCollapsible());
  for (const PhraseTab tab : kPhraseTabs) {
    EXPECT_TRUE(
        splitter->widget(0)->isAncestorOf(waveforms_.Pane(tab)->View()));
    EXPECT_TRUE(
        splitter->widget(1)->isAncestorOf(waveforms_.SettingsArea(tab)));
  }
  // A tab given no settings says so.
  const auto* none = qobject_cast<QLabel*>(
      waveforms_.SettingsArea(PhraseTab::kOriginal)->widget());
  ASSERT_NE(none, nullptr);
  EXPECT_EQ(none->text(), QStringLiteral("No settings"));
}

// The areas keep their sizes whichever tab is shown, so the waveforms are
// at the same place to compare; settings too tall for their area scroll
// rather than growing it.
TEST_F(PhraseWaveformsTest, AreasKeepTheirSizesAcrossTabs) {
  auto* tall = new QLabel(QStringLiteral("tall settings"));
  tall->setMinimumHeight(2000);
  waveforms_.SetControls(PhraseTab::kEncode, tall);
  ShowPhrase();
  ShowTab(PhraseTab::kOriginal);
  const QList<int> sizes = waveforms_.Splitter()->sizes();
  const QRect original = Original()->geometry();
  for (const PhraseTab tab : {PhraseTab::kPreprocess, PhraseTab::kEncode}) {
    ShowTab(tab);
    EXPECT_EQ(waveforms_.Splitter()->sizes(), sizes);
    EXPECT_EQ(waveforms_.Pane(tab)->View()->geometry(), original);
  }
  QScrollArea* area = waveforms_.SettingsArea(PhraseTab::kEncode);
  EXPECT_LT(area->height(), 2000);
  EXPECT_GT(area->verticalScrollBar()->maximum(), 0);
}

// The noise gate is shown on the preprocessed recording, where it mutes: the
// stretches it turns down are shaded and the note gives its level and
// speed.
TEST_F(PhraseWaveformsTest, ShadesWhatTheNoiseGateMutes) {
  // Loud for the first half, 60 dB quieter after.
  std::fill(phrase_.audio.begin() + (kAudioSamples / 2), phrase_.audio.end(),
            0.0001F);
  ShowPhrase();
  WaveformPane* pane = waveforms_.PreprocessedPane();
  EXPECT_TRUE(Preprocessed()->Shading().empty());
  EXPECT_EQ(pane->Note(),
            QStringLiteral("no preprocessing: the same as the original"));

  waveforms_.SetPreprocessing({.noise_gate_db = 30.0});
  ASSERT_THAT(Preprocessed()->Shading(), SizeIs(1));
  const TimeExtent shaded = Preprocessed()->Shading().front();
  // Inside the quiet half, once the gate has begun to close, to the end.
  EXPECT_GT(shaded.begin, kAudioSamples / 2);
  EXPECT_DOUBLE_EQ(shaded.end, kAudioSamples);
  EXPECT_EQ(pane->Note(),
            QStringLiteral(
                "noise gate 30 dB, 100 ms: shaded stretches are turned down"));
  // Neither the original nor the encoded audio is shaded.
  EXPECT_TRUE(Original()->Shading().empty());
  EXPECT_TRUE(waveforms_.OriginalPane()->Note().isEmpty());
  EXPECT_TRUE(Encoded()->Shading().empty());

  // Too low a noise gate to mute anything says so.
  waveforms_.SetPreprocessing({.noise_gate_db = 70.0});
  EXPECT_TRUE(Preprocessed()->Shading().empty());
  EXPECT_EQ(pane->Note(),
            QStringLiteral(
                "noise gate 70 dB, 100 ms: nothing is quiet enough to gate"));

  // The noise gate's note comes first, then the filters.
  waveforms_.SetPreprocessing(
      {.noise_gate_db = 30.0, .noise_gate_ms = 250.0, .treble_tilt = 0.4});
  EXPECT_EQ(pane->Note(),
            QStringLiteral("noise gate 30 dB, 250 ms: shaded stretches are "
                           "turned down · treble tilt 0.4"));

  // Another phrase is shaded by the same noise gate; none clears it.
  waveforms_.SetPreprocessing({.noise_gate_db = 30.0});
  const Phrase other = EncodedPhrase(2, "OTHER", 50);
  waveforms_.ShowPhrase(&other);
  QCoreApplication::processEvents();
  EXPECT_TRUE(Preprocessed()->Shading().empty());
  ShowPhrase();
  EXPECT_THAT(Preprocessed()->Shading(), SizeIs(1));
  waveforms_.SetPreprocessing({});
  EXPECT_TRUE(Preprocessed()->Shading().empty());
  waveforms_.ShowPhrase(nullptr);
  EXPECT_TRUE(pane->Note().isEmpty());
}

// The note names the de-esser, after the filters.
TEST_F(PhraseWaveformsTest, NoteNamesTheDeEsser) {
  ShowPhrase();
  waveforms_.SetPreprocessing({.bass_cut_hz = 200.0, .de_ess_db = 10.0});
  EXPECT_EQ(waveforms_.PreprocessedPane()->Note(),
            QStringLiteral("bass cut 200 Hz, de-esser 10 dB"));
}

// The Preprocess tab shows and plays the recording preprocessed, and
// follows a change of preprocessing, keeping its zoom; the Original tab
// still plays the original.
TEST_F(PhraseWaveformsTest, PreprocessTabShowsAndPlaysThePreprocessedAudio) {
  ShowPhrase();
  ShowTab(PhraseTab::kPreprocess);
  Preprocessed()->ZoomIn();
  const TimeAxis zoomed = Preprocessed()->Axis();

  // A treble tilt of 0.4 takes the steady 0.1 to 0.06.
  waveforms_.SetPreprocessing({.treble_tilt = 0.4});
  EXPECT_EQ(waveforms_.PreprocessedPane()->Note(),
            QStringLiteral("treble tilt 0.4"));
  EXPECT_EQ(Preprocessed()->Axis(), zoomed);
  std::vector<std::int16_t> played;
  EXPECT_CALL(playback_.output, Start(_, Player::kSampleRate))
      .WillRepeatedly(DoAll(SaveArg<0>(&played), Return(core::OkStatus())));
  waveforms_.PlayButton(PlaybackSource::kPreprocessed)->click();
  EXPECT_EQ(playback_.player.Source(), PlaybackSource::kPreprocessed);
  ASSERT_THAT(played, SizeIs(kAudioSamples - kPlayStart));
  EXPECT_NEAR(played.front(), 0.06 * 32768.0, 2.0);

  // A change of preprocessing stops the audio it no longer matches.
  waveforms_.SetPreprocessing({.treble_tilt = 0.2});
  EXPECT_EQ(playback_.player.State(), PlaybackState::kStopped);
  EXPECT_EQ(Preprocessed()->Axis(), zoomed);

  // The original is unchanged, and a change of preprocessing leaves it
  // playing.
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  EXPECT_NEAR(played.front(), 0.1 * 32768.0, 2.0);
  waveforms_.SetPreprocessing({.treble_tilt = 0.4});
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
}

// Preprocessing that would go past full scale is shown and played scaled
// down to fit, and the note says by how much.
TEST_F(PhraseWaveformsTest, PreprocessedRecordingIsScaledToFit) {
  for (std::size_t i = 0; i < phrase_.audio.size(); ++i) {
    phrase_.audio[i] = i % 2 == 0 ? 0.9F : -0.9F;
  }
  ShowPhrase();
  // 0.9 x 1.4 is 2.0 dB over full scale.
  waveforms_.SetPreprocessing({.treble_tilt = 0.4});
  EXPECT_EQ(waveforms_.PreprocessedPane()->Note(),
            QStringLiteral("treble tilt 0.4 · scaled by -2.0 dB to fit"));
}

TEST_F(PhraseWaveformsTest, TabsShowTheSameSoundAtTheSamePlace) {
  ShowPhrase();
  // Every tab scrolls over the union of the three, and starts fitted to
  // it, as Fit shows it.
  const TimeExtent all{.begin = -kFirstFrameStretch,
                       .end = kEncodedSamples - kFirstFrameStretch};
  for (const WaveformView* view : {Original(), Preprocessed(), Encoded()}) {
    EXPECT_EQ(view->Extent(), all);
    EXPECT_TRUE(view->IsFitted());
  }
  EXPECT_EQ(Original()->Axis(), FitAxis(all, Original()->width()));

  Original()->ZoomIn();
  Original()->ScrollBy(120.0);
  EXPECT_EQ(Preprocessed()->Axis(), Original()->Axis());
  EXPECT_EQ(Encoded()->Axis(), Original()->Axis());

  // Shown, another tab draws the same sound at the same place.
  ShowTab(PhraseTab::kEncode);
  EXPECT_EQ(Encoded()->width(), Original()->width());
  EXPECT_EQ(Encoded()->Axis(), Original()->Axis());
  for (const int source : {0, 400, 1000}) {
    EXPECT_DOUBLE_EQ(Original()->XOfSample(source),
                     Encoded()->XOfSample(source + kFirstFrameStretch));
  }
  Encoded()->ZoomIn();
  EXPECT_EQ(Original()->Axis(), Encoded()->Axis());
  EXPECT_EQ(Preprocessed()->Axis(), Encoded()->Axis());
}

// A tab not shown keeps its last size, so fitting one tab fits the others
// to their own widths; shown, each shows the whole phrase.
TEST_F(PhraseWaveformsTest, FittingOneTabFitsEveryTab) {
  ShowPhrase();
  waveforms_.ZoomInButton(PhraseTab::kOriginal)->click();
  EXPECT_FALSE(Encoded()->IsFitted());
  waveforms_.FitButton(PhraseTab::kOriginal)->click();
  EXPECT_TRUE(Preprocessed()->IsFitted());
  EXPECT_TRUE(Encoded()->IsFitted());
  ShowTab(PhraseTab::kPreprocess);
  EXPECT_EQ(Preprocessed()->Axis(),
            FitAxis(Preprocessed()->Extent(), Preprocessed()->width()));
  EXPECT_EQ(Original()->Axis(), Preprocessed()->Axis());
}

TEST_F(PhraseWaveformsTest, EachTabsControlsZoomAndScrollEveryTab) {
  ShowPhrase();
  ShowTab(PhraseTab::kPreprocess);
  const TimeAxis fit = Preprocessed()->Axis();
  waveforms_.ZoomInButton(PhraseTab::kPreprocess)->click();
  EXPECT_DOUBLE_EQ(Preprocessed()->Axis().samples_per_pixel,
                   fit.samples_per_pixel / WaveformView::kZoomStep);
  EXPECT_EQ(Original()->Axis(), Preprocessed()->Axis());
  EXPECT_EQ(Encoded()->Axis(), Preprocessed()->Axis());

  QScrollBar* scroll_bar = waveforms_.ScrollBar(PhraseTab::kPreprocess);
  ASSERT_GT(scroll_bar->maximum(), 0);
  scroll_bar->setValue(scroll_bar->maximum());
  EXPECT_DOUBLE_EQ(Preprocessed()->Axis().XToSample(Preprocessed()->width()),
                   Preprocessed()->Extent().end);
  EXPECT_EQ(Encoded()->Axis(), Preprocessed()->Axis());
  // Scrolling another tab's view moves this tab's scroll bar.
  Encoded()->ScrollBy(-50.0);
  EXPECT_EQ(scroll_bar->value(), scroll_bar->maximum() - 50);

  waveforms_.ZoomOutButton(PhraseTab::kPreprocess)->click();
  waveforms_.FitButton(PhraseTab::kPreprocess)->click();
  EXPECT_EQ(Preprocessed()->Axis(), fit);
  EXPECT_EQ(Original()->Axis(), fit);
}

// Without source audio, only the encoded audio can be zoomed, and there
// is no play start to reset.
TEST_F(PhraseWaveformsTest, ControlsNeedSomethingToActOn) {
  phrase_.audio.clear();
  phrase_.source.reset();
  phrase_.play_start = 0;
  ShowPhrase();
  ASSERT_FALSE(Original()->HasSamples());
  ASSERT_FALSE(Preprocessed()->HasSamples());
  for (const PhraseTab tab : {PhraseTab::kOriginal, PhraseTab::kPreprocess}) {
    EXPECT_FALSE(waveforms_.ZoomInButton(tab)->isEnabled());
    EXPECT_FALSE(waveforms_.FitButton(tab)->isEnabled());
  }
  EXPECT_FALSE(waveforms_.PlayButton(PlaybackSource::kOriginal)->isEnabled());
  EXPECT_FALSE(
      waveforms_.PlayButton(PlaybackSource::kPreprocessed)->isEnabled());
  for (const PhraseTab tab : kPhraseTabs) {
    EXPECT_FALSE(waveforms_.ResetButton(tab)->isEnabled());
  }
  const TimeAxis fit = Encoded()->Axis();
  ASSERT_TRUE(waveforms_.ZoomInButton(PhraseTab::kEncode)->isEnabled());
  waveforms_.ZoomInButton(PhraseTab::kEncode)->click();
  EXPECT_NE(Encoded()->Axis(), fit);
}

// The play start is the phrase's: a click in any tab moves it in every
// tab.
TEST_F(PhraseWaveformsTest, ClickMovesThePlayStartInEveryTab) {
  ShowPhrase();
  EXPECT_EQ(Original()->Marker(), kPlayStart);
  EXPECT_EQ(Preprocessed()->Marker(), kPlayStart);
  EXPECT_EQ(Encoded()->Marker(), kPlayStart);
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), 800)).WillOnce(Return(400));
  emit Preprocessed() -> MarkerRequested(800.4);
  EXPECT_EQ(Original()->Marker(), 800);
  EXPECT_EQ(Preprocessed()->Marker(), 800);
  EXPECT_EQ(Encoded()->Marker(), 800);
  EXPECT_EQ(stack_.count(), 1);
  // Each tab plays from it.
  EXPECT_CALL(playback_.output, Start(SizeIs(kAudioSamples - 800), _)).Times(2);
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  waveforms_.PlayPause(PlaybackSource::kPreprocessed);
  EXPECT_CALL(playback_.output,
              Start(SizeIs(kEncodedSamples - 800 - kFirstFrameStretch), _));
  waveforms_.PlayPause(PlaybackSource::kEncoded);
}

TEST_F(PhraseWaveformsTest, PlayStartIsLimitedToTheAudio) {
  ShowPhrase();
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), kAudioSamples))
      .WillOnce(Return(kPlayStart));
  emit Original() -> MarkerRequested(99999.0);
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), 0))
      .WillOnce(Return(kAudioSamples));
  emit Encoded() -> MarkerRequested(-50.0);
}

TEST_F(PhraseWaveformsTest, ResetMovesThePlayStartToTheBeginning) {
  ShowPhrase();
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), 0)).WillOnce(Return(kPlayStart));
  waveforms_.ResetButton(PhraseTab::kEncode)->click();
  EXPECT_EQ(Original()->Marker(), 0);
  EXPECT_EQ(Encoded()->Marker(), 0);
}

TEST_F(PhraseWaveformsTest, PlayingAudioStartsAgainFromTheNewPlayStart) {
  ShowPhrase();
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  playback_.clock.now_us += 100 * kMillisecond;
  ON_CALL(playback_.output, ProcessedMicroseconds())
      .WillByDefault(Return(100 * kMillisecond));
  playback_.player.Tick();
  EXPECT_EQ(playback_.player.Position(), kPlayStart + 800);
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), 0)).WillOnce(Return(kPlayStart));
  EXPECT_CALL(playback_.output, Start(SizeIs(kAudioSamples), _));
  waveforms_.ResetButton(PhraseTab::kOriginal)->click();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
  EXPECT_EQ(playback_.player.Source(), PlaybackSource::kOriginal);
  EXPECT_EQ(playback_.player.Position(), 0);
}

TEST_F(PhraseWaveformsTest, PausedAudioStopsSoTheNextPlayStartsThere) {
  phrase_.play_start = 0;
  ShowPhrase();
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  playback_.clock.now_us += 100 * kMillisecond;
  ON_CALL(playback_.output, ProcessedMicroseconds())
      .WillByDefault(Return(100 * kMillisecond));
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  ASSERT_EQ(playback_.player.State(), PlaybackState::kPaused);
  // The play start is already at the beginning: no step to undo, but
  // playback still goes back there.
  waveforms_.ResetButton(PhraseTab::kEncode)->click();
  EXPECT_EQ(stack_.count(), 0);
  EXPECT_EQ(playback_.player.State(), PlaybackState::kStopped);
  EXPECT_CALL(playback_.output,
              Start(SizeIs(kEncodedSamples - kFirstFrameStretch), _));
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  EXPECT_EQ(playback_.player.Position(), kFirstFrameStretch);
}

TEST_F(PhraseWaveformsTest, RefusedMoveIsReportedAndChangesNothing) {
  ShowPhrase();
  QString failure;
  QObject::connect(&waveforms_, &PhraseWaveforms::Failed,
                   [&failure](const QString&, const QString& message) {
                     failure = message;
                   });
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), 800))
      .WillOnce(Return(core::OutOfRangeError("outside its audio")));
  waveforms_.MovePlayStart(800.0);
  EXPECT_EQ(failure, QStringLiteral("outside its audio"));
  EXPECT_EQ(Original()->Marker(), kPlayStart);
  EXPECT_EQ(stack_.count(), 0);
}

TEST_F(PhraseWaveformsTest, PlaysTheOriginalFromThePlayStart) {
  ShowPhrase();
  EXPECT_CALL(playback_.output,
              Start(SizeIs(kAudioSamples - kPlayStart), Player::kSampleRate));
  waveforms_.PlayButton(PlaybackSource::kOriginal)->click();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
  EXPECT_EQ(playback_.player.Source(), PlaybackSource::kOriginal);
  EXPECT_TRUE(waveforms_.ShowsPause(PlaybackSource::kOriginal));
  EXPECT_FALSE(waveforms_.ShowsPause(PlaybackSource::kPreprocessed));
  EXPECT_FALSE(waveforms_.ShowsPause(PlaybackSource::kEncoded));
  // Showing pause keeps the button's width.
  QToolButton* button = waveforms_.PlayButton(PlaybackSource::kOriginal);
  EXPECT_EQ(button->accessibleName(), QStringLiteral("Pause the original"));
  const int playing_width = button->sizeHint().width();
  playback_.player.Stop();
  EXPECT_FALSE(waveforms_.ShowsPause(PlaybackSource::kOriginal));
  EXPECT_EQ(button->sizeHint().width(), playing_width);
}

TEST_F(PhraseWaveformsTest, PlaysTheEncodedAudioFromTheSameSound) {
  ShowPhrase();
  // Source sample 400 is encoded sample 525.
  EXPECT_CALL(
      playback_.output,
      Start(SizeIs(kEncodedSamples - kPlayStart - kFirstFrameStretch), _));
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  EXPECT_EQ(playback_.player.Position(), kPlayStart + kFirstFrameStretch);
}

TEST_F(PhraseWaveformsTest, PlayheadIsShownInSourceTime) {
  ShowPhrase();
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  playback_.clock.now_us += 100 * kMillisecond;
  ON_CALL(playback_.output, ProcessedMicroseconds())
      .WillByDefault(Return(100 * kMillisecond));
  playback_.player.Tick();
  // 800 samples on from source sample 400.
  EXPECT_EQ(Encoded()->Playhead(), std::optional<double>(kPlayStart + 800));
  EXPECT_EQ(Original()->Playhead(), std::nullopt);
  EXPECT_EQ(Preprocessed()->Playhead(), std::nullopt);
  playback_.player.Stop();
  EXPECT_EQ(Encoded()->Playhead(), std::nullopt);
}

TEST_F(PhraseWaveformsTest, PlayAgainPausesAndThenResumes) {
  ShowPhrase();
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPaused);
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
  // Another tab takes over.
  waveforms_.PlayPause(PlaybackSource::kPreprocessed);
  EXPECT_EQ(playback_.player.Source(), PlaybackSource::kPreprocessed);
  EXPECT_FALSE(waveforms_.ShowsPause(PlaybackSource::kOriginal));
  EXPECT_TRUE(waveforms_.ShowsPause(PlaybackSource::kPreprocessed));
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  EXPECT_EQ(playback_.player.Source(), PlaybackSource::kEncoded);
}

TEST_F(PhraseWaveformsTest, PlayIconsAreDrawnInTheButtonTextColour) {
  ShowPhrase();
  for (const QPalette& palette :
       {testing::LightPalette(), testing::DarkPalette()}) {
    waveforms_.setPalette(palette);
    const QToolButton* button =
        waveforms_.PlayButton(PlaybackSource::kOriginal);
    const QImage icon = button->icon().pixmap(16, 16).toImage();
    // The middle of the play triangle.
    EXPECT_EQ(QColor(icon.pixel(icon.width() / 2, icon.height() / 2)),
              palette.color(QPalette::ButtonText));
  }
}

TEST_F(PhraseWaveformsTest, EncodedAudioIsShownAndPlayedAtTheOriginalsLevel) {
  // The original: synthetic speech. The chip speaks it at 100 codes per
  // unit after 300 idle codes; a sample is a code x 64 of 32768, so the
  // encoded audio is 32768 / 6400 = 5.12 times (14.2 dB) quieter, less a
  // little for the rounded codes and the high-passed source.
  constexpr int kIdle = 300;
  const std::vector<double> speech = testing::SyntheticSpeech();
  phrase_.audio = core::dsp::ToFloat(speech);
  core::synth::SynthResult spoken = Idle(kIdle);
  for (const double v : speech) {
    const int code = static_cast<int>(std::lround(v * 100.0));
    spoken.codes.push_back(code);
    spoken.samples.push_back(static_cast<std::int16_t>(code * 64));
  }
  spoken.codes.insert(spoken.codes.end(), kIdle, -1);
  spoken.samples.insert(spoken.samples.end(), kIdle, -64);
  ON_CALL(playback_.synth, Synthesize(_, _)).WillByDefault(Return(spoken));
  ShowPhrase();
  EXPECT_NEAR(Encoded()->SourceOffset(), kIdle, 5);
  EXPECT_THAT(waveforms_.EncodedPane()->Note().toStdString(),
              ::testing::HasSubstr("level matched to the original (14."));

  std::vector<std::int16_t> played;
  EXPECT_CALL(playback_.output, Start(_, _))
      .WillOnce([&played](std::vector<std::int16_t> samples, int) {
        played = std::move(samples);
        return core::OkStatus();
      });
  phrase_.play_start = 0;
  waveforms_.ShowPhrase(&phrase_);
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  ASSERT_FALSE(played.empty());
  // The same peak as the original, within a few per cent.
  const std::vector<std::int16_t> original = ToPcm16(phrase_.audio);
  const auto peak = [](const std::vector<std::int16_t>& samples) {
    const auto [low, high] = std::ranges::minmax(samples);
    return std::max(-static_cast<int>(low), static_cast<int>(high));
  };
  EXPECT_NEAR(peak(played), peak(original), peak(original) * 0.03);
}

TEST_F(PhraseWaveformsTest, StaleNoteFollowsTheLevelNote) {
  waveforms_.ShowPhrase(&phrase_, QStringLiteral("stale"));
  QCoreApplication::processEvents();
  // The idle test output has no sound, so it is not level matched.
  EXPECT_EQ(waveforms_.EncodedPane()->Note(), QStringLiteral("stale"));
}

TEST_F(PhraseWaveformsTest, OutputFilterRedrawsTheEncodedAudioInPlace) {
  ShowPhrase();
  waveforms_.ZoomInButton(PhraseTab::kEncode)->click();
  const TimeAxis axis = Encoded()->Axis();
  // Drawn again from the render kept, not spoken again.
  EXPECT_CALL(playback_.synth, Synthesize(_, _)).Times(0);
  waveforms_.SetOutputFilter(core::dsp::OutputFilter::kBbcMicro);
  EXPECT_EQ(waveforms_.EncodedPane()->Note(),
            QStringLiteral("BBC Micro output filter"));
  EXPECT_TRUE(Encoded()->HasSamples());
  EXPECT_EQ(Encoded()->Axis().first, axis.first);
  EXPECT_EQ(Encoded()->Axis().samples_per_pixel, axis.samples_per_pixel);
  waveforms_.SetOutputFilter(core::dsp::OutputFilter::kNone);
  EXPECT_TRUE(waveforms_.EncodedPane()->Note().isEmpty());
}

TEST_F(PhraseWaveformsTest, SpeakerPinClipsWhatTheIoPinKeeps) {
  // No source audio, so no level match: the samples play as spoken.
  phrase_.audio.clear();
  phrase_.source.reset();
  phrase_.play_start = 0;
  core::synth::SynthResult loud = Idle(kEncodedSamples);
  loud.codes[100] = 300;
  loud.samples[100] = 300 * core::synth::kSampleScale;
  loud.codes[101] = -300;
  loud.samples[101] = -300 * core::synth::kSampleScale;
  EXPECT_CALL(playback_.synth, Synthesize(_, _)).WillOnce(Return(loud));
  ShowPhrase();
  EXPECT_EQ(waveforms_.EncodedPane()->Note(),
            QStringLiteral("2 samples clipped by the DAC"));

  std::vector<std::int16_t> played;
  EXPECT_CALL(playback_.output, Start(_, _))
      .WillRepeatedly(DoAll(SaveArg<0>(&played), Return(core::OkStatus())));
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  ASSERT_FALSE(played.empty());
  EXPECT_NEAR(*std::ranges::max_element(played),
              127 * core::synth::kSampleScale, 1);
  EXPECT_NEAR(*std::ranges::min_element(played),
              -128 * core::synth::kSampleScale, 1);

  playback_.player.Stop();
  waveforms_.SetOutputPin(core::synth::OutputPin::kIo);
  EXPECT_EQ(waveforms_.EncodedPane()->Note(),
            QStringLiteral("I/O pin, 10-bit"));
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  EXPECT_NEAR(*std::ranges::max_element(played),
              300 * core::synth::kSampleScale, 1);
  EXPECT_NEAR(*std::ranges::min_element(played),
              -300 * core::synth::kSampleScale, 1);
}

TEST_F(PhraseWaveformsTest, DragSelectsInEveryTabAndPlayPlaysJustThat) {
  ShowPhrase();
  emit Encoded() -> SelectionRequested(200.4, 1000.6, true);
  const TimeExtent selected{.begin = 200.0, .end = 1001.0};
  for (const WaveformView* view : {Original(), Preprocessed(), Encoded()}) {
    EXPECT_EQ(view->Selection(), selected);
  }
  // Not part of the project: nothing to undo.
  EXPECT_EQ(stack_.count(), 0);
  EXPECT_CALL(playback_.output, Start(SizeIs(801), _)).Times(3);
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  EXPECT_EQ(playback_.player.Position(), 200);
  waveforms_.PlayPause(PlaybackSource::kPreprocessed);
  // The encoded audio plays the same sound.
  waveforms_.PlayPause(PlaybackSource::kEncoded);
  EXPECT_EQ(playback_.player.Position(), 200 + kFirstFrameStretch);
}

TEST_F(PhraseWaveformsTest, SelectionIsLimitedToTheAudio) {
  ShowPhrase();
  waveforms_.Select(-50.0, 99999.0);
  EXPECT_EQ(waveforms_.Selection(),
            (TimeExtent{.begin = 0.0, .end = kAudioSamples}));
  // Less than a sample selects nothing.
  waveforms_.Select(300.0, 300.2);
  EXPECT_EQ(waveforms_.Selection(), std::nullopt);
  EXPECT_EQ(Original()->Selection(), std::nullopt);
}

TEST_F(PhraseWaveformsTest, ClickEscapeOrAnotherPhraseClearsTheSelection) {
  ShowPhrase();
  waveforms_.Select(200.0, 1000.0);
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), 800)).WillOnce(Return(400));
  emit Original() -> MarkerRequested(800.0);
  EXPECT_EQ(waveforms_.Selection(), std::nullopt);
  EXPECT_EQ(Encoded()->Selection(), std::nullopt);

  waveforms_.Select(200.0, 1000.0);
  emit Preprocessed() -> SelectionClearRequested();
  EXPECT_EQ(waveforms_.Selection(), std::nullopt);

  waveforms_.Select(200.0, 1000.0);
  const Phrase other = EncodedPhrase(2, "OTHER", 50);
  waveforms_.ShowPhrase(&other);
  EXPECT_EQ(waveforms_.Selection(), std::nullopt);
  EXPECT_EQ(Original()->Selection(), std::nullopt);
}

TEST_F(PhraseWaveformsTest, PlaybackFollowsASelectionOnceItIsMade) {
  ShowPhrase();
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  // Still being dragged: playback goes on.
  waveforms_.Select(200.0, 600.0, false);
  EXPECT_EQ(playback_.player.Position(), kPlayStart);
  EXPECT_CALL(playback_.output, Start(SizeIs(600), _));
  waveforms_.Select(200.0, 800.0);
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
  EXPECT_EQ(playback_.player.Position(), 200);
}

TEST_F(PhraseWaveformsTest, ZoomInShowsTheSelectionInEveryTab) {
  ShowPhrase();
  waveforms_.Select(200.0, 1000.0);
  waveforms_.ZoomInButton(PhraseTab::kOriginal)->click();
  EXPECT_DOUBLE_EQ(Original()->Axis().first, 200.0);
  EXPECT_DOUBLE_EQ(Original()->Axis().XToSample(Original()->width()), 1000.0);
  EXPECT_EQ(Encoded()->Axis(), Original()->Axis());
}

TEST_F(PhraseWaveformsTest, AnotherPhraseStopsPlayback) {
  ShowPhrase();
  waveforms_.PlayPause(PlaybackSource::kOriginal);
  const Phrase other = EncodedPhrase(2, "OTHER", 50);
  waveforms_.ShowPhrase(&other);
  EXPECT_EQ(playback_.player.State(), PlaybackState::kStopped);
}

TEST_F(PhraseWaveformsTest, PhraseNotEncodedSaysSo) {
  phrase_.encoding.reset();
  EXPECT_CALL(playback_.synth, Synthesize(_, _)).Times(0);
  ShowPhrase();
  EXPECT_FALSE(Encoded()->HasSamples());
  EXPECT_TRUE(Encoded()->Placeholder().contains(QStringLiteral("Not encoded")));
  EXPECT_FALSE(waveforms_.PlayButton(PlaybackSource::kEncoded)->isEnabled());
  EXPECT_FALSE(waveforms_.ZoomInButton(PhraseTab::kEncode)->isEnabled());
  EXPECT_TRUE(waveforms_.PlayButton(PlaybackSource::kOriginal)->isEnabled());
  EXPECT_TRUE(
      waveforms_.PlayButton(PlaybackSource::kPreprocessed)->isEnabled());
}

TEST_F(PhraseWaveformsTest, FailedRenderSaysWhy) {
  EXPECT_CALL(playback_.synth, Synthesize(_, _))
      .WillOnce(Return(core::InternalError("the Speak command did not start")));
  ShowPhrase();
  EXPECT_FALSE(Encoded()->HasSamples());
  EXPECT_TRUE(Encoded()->Placeholder().contains(
      QStringLiteral("the Speak command did not start")));
}

TEST_F(PhraseWaveformsTest, NoPhraseClearsEveryTab) {
  ShowPhrase();
  waveforms_.ShowPhrase(nullptr);
  EXPECT_FALSE(Original()->HasSamples());
  EXPECT_FALSE(Preprocessed()->HasSamples());
  EXPECT_FALSE(Encoded()->HasSamples());
  EXPECT_FALSE(waveforms_.PlayButton(PlaybackSource::kOriginal)->isEnabled());
  EXPECT_FALSE(waveforms_.PlayButton(PlaybackSource::kEncoded)->isEnabled());
  EXPECT_FALSE(waveforms_.ResetButton(PhraseTab::kOriginal)->isEnabled());
}

TEST_F(PhraseWaveformsTest, EncodedNoteIsShownBesideTheTitle) {
  waveforms_.ShowPhrase(&phrase_, QStringLiteral("stale"));
  EXPECT_EQ(waveforms_.EncodedPane()->Note(), QStringLiteral("stale"));
}

}  // namespace
}  // namespace speak5220::app
