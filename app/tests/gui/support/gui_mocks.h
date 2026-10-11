/*
 * File:        gui_mocks.h
 * Module:      gui tests
 * Purpose:     gMock mocks of the main window's dialogs and the audio
 *              output, an in-memory recent-file store, a fake clock, and
 *              the playback the main window needs
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_GUI_SUPPORT_GUI_MOCKS_H_
#define SPEAK5220_TESTS_GUI_SUPPORT_GUI_MOCKS_H_

#include <QString>
#include <QStringList>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/project/phrase.h"
#include "core/quality/quality_metric.h"
#include "core/util/clock.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gui/encoder_panel/phrase_encoder.h"
#include "gui/main_window/dialogs.h"
#include "gui/main_window/recent_files.h"
#include "gui/main_window/view_preferences.h"
#include "gui/playback/audio_output.h"
#include "gui/playback/player.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"
#include "support/quality_mocks.h"
#include "support/synth_mocks.h"

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

class FakeViewPreferences : public app::IViewPreferences {
 public:
  bool AdvancedSettingsShown() override { return advanced_shown; }
  void SetAdvancedSettingsShown(bool shown) override { advanced_shown = shown; }

  bool advanced_shown = false;
};

class MockAudioOutput : public app::IAudioOutput {
 public:
  MOCK_METHOD(int, PlaybackRate, (int rate), (override));
  MOCK_METHOD(core::Status, Start,
              (std::vector<std::int16_t> samples, int rate), (override));
  MOCK_METHOD(void, Stop, (), (override));
  MOCK_METHOD(std::int64_t, ProcessedMicroseconds, (), (override));
  MOCK_METHOD(bool, IsFinished, (), (override));
};

// A clock that moves only when told to.
class FakeClock : public core::IClock {
 public:
  std::int64_t NowMicroseconds() const override { return now_us; }

  std::int64_t now_us = 0;
};

// A Player over a mocked output that takes 8 kHz, starts, and never
// finishes, with a fake clock and a mocked resampler; and a mocked synth
// that speaks any phrase as kSpokenSamples idle samples. For main windows
// and waveforms that are not testing playback.
struct TestPlayback {
  static constexpr int kSpokenSamples = 400;

  TestPlayback() {
    ON_CALL(output, PlaybackRate(::testing::_))
        .WillByDefault(::testing::ReturnArg<0>());
    ON_CALL(output, Start(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(core::OkStatus()));
    ON_CALL(resampler, Resample(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(
            core::InternalError("the test resampler is not set up")));
    core::synth::SynthResult idle;
    idle.codes.assign(kSpokenSamples, -1);
    idle.samples.assign(kSpokenSamples, -64);
    idle.leading_idle_samples = kSpokenSamples;
    ON_CALL(synth, Synthesize(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(idle));
  }

  ::testing::NiceMock<MockAudioOutput> output;
  FakeClock clock;
  ::testing::NiceMock<MockResampler> resampler;
  ::testing::NiceMock<MockSynth> synth;
  app::Player player{output, clock, resampler};
};

// Encoding tools over mocks, for windows and panels that are not testing
// them: an encoder whose frames pack to kEncodedBytes whatever the
// options, a PESQ of kScore, an anchor of kAnchor, a fake clock, and an
// inline runner for Optimise's search.
struct TestEncoding {
  static constexpr int kEncodedBytes = 20;
  static constexpr double kScore = 2.0;
  static constexpr double kAnchor = 2.5;

  TestEncoding() {
    ON_CALL(encoder, Encode(::testing::_, ::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(core::encoder::EncodeResult{
            .frames = FramesPackingTo(kEncodedBytes),
            .details = std::nullopt}));
    ON_CALL(quality, Measure(::testing::_, ::testing::_))
        .WillByDefault(::testing::Return(kScore));
    ON_CALL(quality, Direction())
        .WillByDefault(
            ::testing::Return(core::quality::MetricDirection::kHigherIsBetter));
    ON_CALL(anchor, Score(::testing::_))
        .WillByDefault(::testing::Return(kAnchor));
  }

  app::EncodingTools Tools() {
    return {.encoder = &encoder,
            .quality = &quality,
            .anchor = &anchor,
            .clock = &clock,
            .optimiser_driver = &driver};
  }

  ::testing::NiceMock<MockEncoder> encoder;
  ::testing::NiceMock<MockQualityMetric> quality;
  ::testing::NiceMock<MockAnchorScorer> anchor;
  FakeClock clock;
  core::InlineTaskRunner driver;
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
