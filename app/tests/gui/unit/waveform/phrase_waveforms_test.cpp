/*
 * File:        phrase_waveforms_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of the two waveform panes with a mocked editor,
 *              synth and audio output: rendering, linked time axes, the
 *              play-start marker and playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/phrase_waveforms.h"

#include <QCheckBox>
#include <QColor>
#include <QCoreApplication>
#include <QImage>
#include <QObject>
#include <QPalette>
#include <QScrollBar>
#include <QString>
#include <QToolButton>
#include <QUndoStack>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <utility>
#include <vector>

#include "core/dsp/output_filter.h"
#include "core/project/phrase.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/playback/player.h"
#include "gui/support/gui_mocks.h"
#include "gui/support/test_palettes.h"
#include "gui/undo/project_edits.h"
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
using testing::EncodedPhrase;
using testing::MockProjectEditor;
using ::testing::Return;
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

  WaveformView* Original() const { return waveforms_.OriginalPane()->View(); }
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

TEST_F(PhraseWaveformsTest, ShadesWhatTheSquelchMutes) {
  // Loud for the first half, 60 dB quieter after.
  std::fill(phrase_.audio.begin() + (kAudioSamples / 2), phrase_.audio.end(),
            0.0001F);
  ShowPhrase();
  EXPECT_TRUE(Original()->Shading().empty());
  EXPECT_TRUE(waveforms_.OriginalPane()->Note().isEmpty());

  waveforms_.SetSquelch(30.0);
  ASSERT_THAT(Original()->Shading(), SizeIs(1));
  const TimeExtent shaded = Original()->Shading().front();
  // Inside the quiet half, after the hold, to the end.
  EXPECT_GT(shaded.begin, kAudioSamples / 2);
  EXPECT_DOUBLE_EQ(shaded.end, kAudioSamples);
  EXPECT_EQ(waveforms_.OriginalPane()->Note(),
            QStringLiteral("squelch 30 dB: shaded stretches are muted"));
  EXPECT_TRUE(Encoded()->Shading().empty());

  // Too low a squelch to mute anything says so.
  waveforms_.SetSquelch(70.0);
  EXPECT_TRUE(Original()->Shading().empty());
  EXPECT_EQ(waveforms_.OriginalPane()->Note(),
            QStringLiteral("squelch 70 dB: nothing is quiet enough to mute"));

  // Another phrase is shaded by the same squelch; none clears it.
  waveforms_.SetSquelch(30.0);
  const Phrase other = EncodedPhrase(2, "OTHER", 50);
  waveforms_.ShowPhrase(&other);
  QCoreApplication::processEvents();
  EXPECT_TRUE(Original()->Shading().empty());
  ShowPhrase();
  EXPECT_THAT(Original()->Shading(), SizeIs(1));
  waveforms_.SetSquelch(std::nullopt);
  EXPECT_TRUE(Original()->Shading().empty());
  EXPECT_TRUE(waveforms_.OriginalPane()->Note().isEmpty());
}

TEST_F(PhraseWaveformsTest, PanesShowTheSameSoundAtTheSamePlace) {
  ShowPhrase();
  // The panes are stacked, the same size (an odd pixel of height goes to
  // one of them).
  EXPECT_EQ(Original()->width(), Encoded()->width());
  EXPECT_LE(std::abs(Original()->height() - Encoded()->height()), 1);
  // Both scroll over the union of the two.
  const TimeExtent both{.begin = -kFirstFrameStretch,
                        .end = kEncodedSamples - kFirstFrameStretch};
  EXPECT_EQ(Original()->Extent(), both);
  EXPECT_EQ(Encoded()->Extent(), both);
  // They start fitted to it, as Fit shows them.
  const TimeAxis fit = FitAxis(both, Original()->width());
  EXPECT_EQ(Original()->Axis(), fit);
  EXPECT_EQ(Encoded()->Axis(), fit);

  Original()->ZoomIn();
  Original()->ScrollBy(120.0);
  EXPECT_EQ(Encoded()->Axis(), Original()->Axis());
  for (const int source : {0, 400, 1000}) {
    EXPECT_DOUBLE_EQ(Original()->XOfSample(source),
                     Encoded()->XOfSample(source + kFirstFrameStretch));
  }
  Encoded()->ZoomIn();
  EXPECT_EQ(Original()->Axis(), Encoded()->Axis());
}

TEST_F(PhraseWaveformsTest, SharedControlsZoomAndScrollBoth) {
  ShowPhrase();
  const TimeAxis fit = Original()->Axis();
  waveforms_.ZoomInButton()->click();
  EXPECT_DOUBLE_EQ(Original()->Axis().samples_per_pixel,
                   fit.samples_per_pixel / WaveformView::kZoomStep);
  EXPECT_EQ(Encoded()->Axis(), Original()->Axis());

  QScrollBar* scroll_bar = waveforms_.ScrollBar();
  ASSERT_GT(scroll_bar->maximum(), 0);
  scroll_bar->setValue(scroll_bar->maximum());
  EXPECT_DOUBLE_EQ(Original()->Axis().XToSample(Original()->width()),
                   Original()->Extent().end);
  EXPECT_EQ(Encoded()->Axis(), Original()->Axis());
  // Scrolling a view moves the scroll bar.
  Encoded()->ScrollBy(-50.0);
  EXPECT_EQ(scroll_bar->value(), scroll_bar->maximum() - 50);

  waveforms_.ZoomOutButton()->click();
  waveforms_.FitButton()->click();
  EXPECT_EQ(Original()->Axis(), fit);
  EXPECT_EQ(Encoded()->Axis(), fit);
}

TEST_F(PhraseWaveformsTest, SharedControlsZoomTheEncodedAudioAlone) {
  phrase_.audio.clear();
  phrase_.source.reset();
  phrase_.play_start = 0;
  ShowPhrase();
  ASSERT_FALSE(Original()->HasSamples());
  const TimeAxis fit = Encoded()->Axis();
  waveforms_.ZoomInButton()->click();
  EXPECT_NE(Encoded()->Axis(), fit);
  EXPECT_FALSE(waveforms_.ResetButton()->isEnabled());
}

TEST_F(PhraseWaveformsTest, ClickMovesThePlayStartInBothPanes) {
  ShowPhrase();
  EXPECT_EQ(Original()->Marker(), kPlayStart);
  EXPECT_CALL(editor_, SetPlayStart(TestId(1), 800)).WillOnce(Return(400));
  emit Encoded() -> MarkerRequested(800.4);
  EXPECT_EQ(Original()->Marker(), 800);
  EXPECT_EQ(Encoded()->Marker(), 800);
  EXPECT_EQ(stack_.count(), 1);
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
  waveforms_.ResetButton()->click();
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
  waveforms_.ResetButton()->click();
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
  waveforms_.ResetButton()->click();
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
  EXPECT_FALSE(waveforms_.ShowsPause(PlaybackSource::kEncoded));
  // Showing pause keeps the button's label, and so its width.
  QToolButton* button = waveforms_.PlayButton(PlaybackSource::kOriginal);
  EXPECT_EQ(button->text(), QStringLiteral("Original"));
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
  // The other pane takes over.
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
  phrase_.audio.assign(speech.begin(), speech.end());
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
  waveforms_.ZoomInButton()->click();
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
  EXPECT_TRUE(waveforms_.PlayButton(PlaybackSource::kOriginal)->isEnabled());
}

TEST_F(PhraseWaveformsTest, FailedRenderSaysWhy) {
  EXPECT_CALL(playback_.synth, Synthesize(_, _))
      .WillOnce(Return(core::InternalError("the Speak command did not start")));
  ShowPhrase();
  EXPECT_FALSE(Encoded()->HasSamples());
  EXPECT_TRUE(Encoded()->Placeholder().contains(
      QStringLiteral("the Speak command did not start")));
}

TEST_F(PhraseWaveformsTest, NoPhraseClearsBothPanes) {
  ShowPhrase();
  waveforms_.ShowPhrase(nullptr);
  EXPECT_FALSE(Original()->HasSamples());
  EXPECT_FALSE(Encoded()->HasSamples());
  EXPECT_FALSE(waveforms_.PlayButton(PlaybackSource::kOriginal)->isEnabled());
}

TEST_F(PhraseWaveformsTest, EncodedNoteIsShownBesideTheTitle) {
  waveforms_.ShowPhrase(&phrase_, QStringLiteral("stale"));
  EXPECT_EQ(waveforms_.EncodedPane()->Note(), QStringLiteral("stale"));
}

}  // namespace
}  // namespace speak5220::app
