/*
 * File:        encoder_panel_test.cpp
 * Module:      encoder panel tests
 * Purpose:     Unit tests of the encoder panel with a mocked encoder and
 *              quality assessor, a fake clock and an inline runner
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/encoder_panel.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QMetaObject>
#include <QObject>
#include <QProgressBar>
#include <QPushButton>
#include <QString>
#include <QUndoStack>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/preprocess/preprocess.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/quality/cached_quality.h"
#include "core/quality/quality_assessor.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/encoder_panel/option_slider.h"
#include "gui/encoder_panel/quality_rating_view.h"
#include "gui/support/gui_mocks.h"
#include "gui/undo/project_edits.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"
#include "support/quality_mocks.h"

namespace speak5220::app {
namespace {

using core::encoder::EncoderOptions;
using ::testing::_;
using ::testing::AnyNumber;
using testing::AudioPhrase;
using testing::EncodedPhrase;
using ::testing::Field;
using testing::LockedPhrase;
using testing::ProjectWith;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestEncoding;
using testing::TestId;
using testing::TestPlayback;

constexpr std::int64_t kMillisecond = 1000;
constexpr int kSampleRate = 8000;

class EncoderPanelTest : public ::testing::Test {
 public:
  EncoderPanelTest(const EncoderPanelTest&) = delete;
  EncoderPanelTest& operator=(const EncoderPanelTest&) = delete;
  EncoderPanelTest(EncoderPanelTest&&) = delete;
  EncoderPanelTest& operator=(EncoderPanelTest&&) = delete;
  // The stack signals as it is destroyed, after the panel.
  ~EncoderPanelTest() override { QObject::disconnect(connection_); }

 protected:
  EncoderPanelTest() {
    // As the window does: every change of the project is shown again.
    connection_ = QObject::connect(&stack_, &QUndoStack::indexChanged, [this] {
      panel_.ShowPhrase(project_, shown_);
    });
  }

  // Shows phrase `n` and lets its measurement come back.
  void Show(int n) {
    shown_ = TestId(n);
    panel_.ShowPhrase(project_, shown_);
    QCoreApplication::processEvents();
  }

  OptionSlider* Slider(const char* field) const {
    return panel_.Form()->Slider(field);
  }

  void Advance(std::int64_t ms) { encoding_.clock.now_us += ms * kMillisecond; }

  // The time passes and the panel's timer fires.
  void Tick(std::int64_t ms) {
    Advance(ms);
    panel_.DebounceTick();
    QCoreApplication::processEvents();
  }

  const core::project::Phrase& Phrase(int n) const {
    return *project_.FindPhrase(TestId(n));
  }

  core::project::Project project_ =
      ProjectWith({EncodedPhrase(1, "HELLO", 100), AudioPhrase(2, "WORLD"),
                   LockedPhrase(3, "LOCKED", 50)});
  QUndoStack stack_;
  ProjectEdits edits_{project_, stack_};
  TestPlayback playback_;
  TestEncoding encoding_;
  core::InlineTaskRunner runner_;
  core::project::PhraseId shown_;
  QMetaObject::Connection connection_;
  EncoderPanel panel_{edits_, encoding_.Tools(), playback_.synth, runner_};
};

TEST_F(EncoderPanelTest, ShowsTheStatisticsAndTheMeasuredQuality) {
  Show(1);
  EXPECT_TRUE(panel_.FramesLabel()->text().contains(QStringLiteral("frames")));
  EXPECT_TRUE(
      panel_.KindsLabel()->text().startsWith(QStringLiteral("voiced ")));
  // The test synth's output is idle (code -1) throughout.
  EXPECT_EQ(panel_.LevelLabel()->text(), QStringLiteral("peak code 1 of 127"));
  EXPECT_EQ(panel_.Quality()->Text(), QStringLiteral("Fair · 0.60"));
  EXPECT_FALSE(panel_.Quality()->Figures().isEmpty());
  EXPECT_EQ(panel_.JobsRunning(), 0);
}

TEST_F(EncoderPanelTest, ASettingChangeReEncodesOnceAfterTheDebounce) {
  Show(1);
  EXPECT_CALL(encoding_.encoder, Encode(_, _, _)).Times(0);
  EXPECT_CALL(encoding_.encoder,
              Encode(_, kSampleRate, Field(&EncoderOptions::target_rms, 23.0)))
      .Times(1);
  Slider("target_rms")->MoveTo(22.0);
  Advance(100);
  Slider("target_rms")->MoveTo(23.0);
  EXPECT_TRUE(panel_.HasPendingChange());
  // 400 ms after the last change: not yet.
  Tick(400);
  EXPECT_TRUE(panel_.HasPendingChange());
  EXPECT_EQ(stack_.count(), 0);
  Tick(EncoderPanel::kDebounceMs);
  EXPECT_FALSE(panel_.HasPendingChange());
  EXPECT_EQ(panel_.JobsRunning(), 0);
  EXPECT_EQ(Phrase(1).encoder,
            (core::project::EncoderOverrides{{"target_rms", "23"}}));
  EXPECT_EQ(stack_.count(), 1);
  EXPECT_DOUBLE_EQ(Slider("target_rms")->Value(), 23.0);
  // Ticks after that do nothing.
  Tick(EncoderPanel::kDebounceMs);
  EXPECT_EQ(stack_.count(), 1);
}

// The quality is measured against the recording as preprocessed.
TEST_F(EncoderPanelTest, PhraseIsMeasuredAgainstItsPreprocessedSource) {
  // Loud, then 60 dB quieter, half a second each.
  core::project::Phrase breath = AudioPhrase(4, "BREATH");
  breath.audio.assign(8000, 0.5F);
  std::fill(breath.audio.begin() + 4000, breath.audio.end(), 0.0005F);
  project_ = ProjectWith({breath});
  const std::vector<double> source(breath.audio.begin(), breath.audio.end());
  std::vector<double> measured;
  ON_CALL(encoding_.quality, Assess(_, _))
      .WillByDefault([&measured](std::span<const double> s,
                                 std::span<const core::lpc::Frame>) {
        measured.assign(s.begin(), s.end());
        return core::StatusOr<core::quality::QualityAssessment>(
            testing::AssessmentScoring(TestEncoding::kScore));
      });
  Show(4);
  panel_.ReEncodeButton()->click();
  QCoreApplication::processEvents();
  EXPECT_EQ(measured, source);

  panel_.Form()->OnBox("noise_gate_db")->setChecked(true);
  Tick(EncoderPanel::kDebounceMs);
  const std::vector<double> gated =
      core::preprocess::Preprocess(source, {.noise_gate_db = 30.0}).samples;
  ASSERT_NE(gated, source);
  EXPECT_EQ(measured, gated);

  panel_.Form()->OnBox("noise_gate_db")->setChecked(false);
  Tick(EncoderPanel::kDebounceMs);
  EXPECT_EQ(measured, source);

  // The BBC Micro preset: a bass cut and a treble tilt.
  panel_.Form()->PresetBox()->activated(1);
  Tick(EncoderPanel::kDebounceMs);
  EXPECT_EQ(measured,
            core::preprocess::Preprocess(
                source, core::preprocess::PreprocessPresets()[1].options)
                .samples);
}

TEST_F(EncoderPanelTest, ReportsThePreprocessingAsTheFormHasIt) {
  std::vector<std::optional<double>> reported;
  QObject::connect(
      &panel_, &EncoderPanel::PreprocessingChanged,
      [&reported](const core::preprocess::PreprocessOptions& options) {
        reported.push_back(options.noise_gate_db);
      });
  Show(2);
  EXPECT_EQ(panel_.ShownPreprocessing(), core::preprocess::PreprocessOptions{});
  EXPECT_TRUE(reported.empty());
  // At once, before the change is applied, so the waveform can show it.
  panel_.Form()->OnBox("noise_gate_db")->setChecked(true);
  EXPECT_EQ(reported, (std::vector<std::optional<double>>{30.0}));
  Slider("noise_gate_db")->MoveTo(24.0);
  EXPECT_EQ(reported.back(), 24.0);
  Tick(EncoderPanel::kDebounceMs);
  EXPECT_EQ(reported.size(), 2U);
  Slider("treble_tilt")->MoveTo(0.4);
  EXPECT_EQ(panel_.ShownPreprocessing().treble_tilt, 0.4);
  EXPECT_EQ(reported.size(), 3U);

  // The project's defaults do not change a noise gate the phrase sets.
  panel_.SetScope(EncoderPanel::Scope::kProjectDefaults);
  panel_.Form()->OnBox("noise_gate_db")->setChecked(true);
  EXPECT_EQ(panel_.ShownPreprocessing().noise_gate_db, 24.0);
  panel_.SetScope(EncoderPanel::Scope::kPhrase);

  // A locked phrase is never encoded, so it has none.
  Show(3);
  EXPECT_EQ(panel_.ShownPreprocessing(), core::preprocess::PreprocessOptions{});
  EXPECT_EQ(reported.back(), std::nullopt);
}

TEST_F(EncoderPanelTest, StoresTheQualityWithTheEncoding) {
  Show(2);
  EXPECT_EQ(panel_.ReEncodeButton()->text(), QStringLiteral("&Encode"));
  EXPECT_EQ(panel_.StateLabel()->text(), QStringLiteral("Not encoded yet"));
  panel_.ReEncodeButton()->click();
  QCoreApplication::processEvents();
  ASSERT_TRUE(Phrase(2).encoding.has_value());
  ASSERT_TRUE(Phrase(2).encoding->quality.has_value());
  EXPECT_NEAR(Phrase(2).encoding->quality->score, TestEncoding::kScore, 1e-12);
  EXPECT_EQ(Phrase(2).encoding->quality->measures_version,
            core::quality::kMeasuresVersion);
  EXPECT_EQ(panel_.StateLabel()->text(), QStringLiteral("Up to date"));
  EXPECT_EQ(panel_.ReEncodeButton()->text(), QStringLiteral("Re-&encode"));
}

TEST_F(EncoderPanelTest, ReEncodeIsOfferedOnlyWhenThereIsSomethingToRedo) {
  Show(2);
  ASSERT_TRUE(panel_.ReEncodeButton()->isEnabled());
  panel_.ReEncode();
  QCoreApplication::processEvents();
  // Up to date: nothing to redo.
  EXPECT_FALSE(panel_.ReEncodeButton()->isEnabled());
  EXPECT_CALL(encoding_.encoder, Encode(_, _, _)).Times(0);
  panel_.ReEncode();
  QCoreApplication::processEvents();
  ::testing::Mock::VerifyAndClearExpectations(&encoding_.encoder);
  // A change waiting to be applied can be applied at once.
  Slider("gate_db")->MoveTo(50.0);
  EXPECT_TRUE(panel_.ReEncodeButton()->isEnabled());
  // New project defaults make it stale.
  panel_.ReEncode();
  QCoreApplication::processEvents();
  EncoderOptions defaults;
  defaults.target_rms = 25.0;
  ASSERT_TRUE(edits_.SetDefaultEncoder(defaults).IsOk());
  EXPECT_EQ(
      panel_.StateLabel()->text(),
      QStringLiteral("Stale: made with other settings; re-encode to update"));
  EXPECT_TRUE(panel_.ReEncodeButton()->isEnabled());
}

TEST_F(EncoderPanelTest, ShowsTheRatingAndItsComponents) {
  ON_CALL(encoding_.quality, Assess(_, _))
      .WillByDefault(Return(testing::AssessmentScoring(0.8)));
  Show(2);
  panel_.ReEncode();
  QCoreApplication::processEvents();
  EXPECT_EQ(panel_.Quality()->Text(), QStringLiteral("Good · 0.80"));
  ASSERT_TRUE(panel_.Quality()->Bar()->Score().has_value());
  EXPECT_NEAR(*panel_.Quality()->Bar()->Score(), 0.8, 1e-9);
  EXPECT_EQ(panel_.Quality()
                ->ComponentBar(QualityRatingView::Component::kIntelligibility)
                ->format(),
            QStringLiteral("0.80"));
}

TEST_F(EncoderPanelTest, CachedQualityIsShownWithoutMeasuring) {
  core::project::CachedEncoding cached = *Phrase(1).encoding;
  cached.quality =
      core::quality::ToCachedQuality(testing::AssessmentScoring(0.3));
  ASSERT_TRUE(project_
                  .SetEncoding(TestId(1), {.overrides = Phrase(1).encoder,
                                           .encoding = cached})
                  .IsOk());
  Show(1);
  EXPECT_EQ(panel_.Quality()->Text(), QStringLiteral("Poor · 0.30"));
}

// Reset puts both the preprocessing and the encoder's options back.
TEST_F(EncoderPanelTest, ResetRestoresTheProjectDefaults) {
  const core::project::EncoderOverrides overrides = {{"target_rms", "18"},
                                                     {"treble_tilt", "0.4"}};
  ASSERT_TRUE(project_
                  .SetEncoding(TestId(1), {.overrides = overrides,
                                           .encoding = Phrase(1).encoding})
                  .IsOk());
  Show(1);
  EXPECT_DOUBLE_EQ(Slider("target_rms")->Value(), 18.0);
  EXPECT_TRUE(panel_.Form()->IsMarked("target_rms"));
  EXPECT_TRUE(panel_.Form()->IsMarked("treble_tilt"));
  ASSERT_TRUE(panel_.ResetButton()->isEnabled());
  EXPECT_CALL(encoding_.encoder,
              Encode(_, kSampleRate, Field(&EncoderOptions::target_rms, 20.0)))
      .Times(1);
  panel_.ResetButton()->click();
  QCoreApplication::processEvents();
  EXPECT_TRUE(Phrase(1).encoder.empty());
  EXPECT_DOUBLE_EQ(Slider("target_rms")->Value(), 20.0);
  EXPECT_DOUBLE_EQ(Slider("treble_tilt")->Value(), 0.0);
  EXPECT_EQ(panel_.Form()->PresetBox()->currentIndex(), 0);
  EXPECT_FALSE(panel_.ResetButton()->isEnabled());
  // One step, undone as one.
  ASSERT_EQ(stack_.count(), 1);
  stack_.undo();
  EXPECT_EQ(Phrase(1).encoder, overrides);
}

TEST_F(EncoderPanelTest, ShowingAnotherPhraseAppliesAPendingChange) {
  Show(1);
  Slider("gate_db")->MoveTo(50.0);
  Show(2);
  EXPECT_FALSE(panel_.HasPendingChange());
  EXPECT_EQ(Phrase(1).encoder,
            (core::project::EncoderOverrides{{"gate_db", "50"}}));
  EXPECT_DOUBLE_EQ(Slider("gate_db")->Value(), 45.0);
}

TEST_F(EncoderPanelTest, ProjectDefaultsAreEditedAsOneStep) {
  Show(1);
  panel_.SetScope(EncoderPanel::Scope::kProjectDefaults);
  EXPECT_EQ(panel_.ScopeBox()->currentIndex(), 1);
  EXPECT_CALL(encoding_.encoder, Encode(_, _, _)).Times(0);
  Slider("target_rms")->MoveTo(25.0);
  Tick(EncoderPanel::kDebounceMs);
  EXPECT_DOUBLE_EQ(project_.Settings().default_encoder.target_rms, 25.0);
  ASSERT_EQ(stack_.count(), 1);
  EXPECT_EQ(stack_.text(0), QStringLiteral("Change Project Defaults"));
  // The encoder's own default is the baseline here.
  EXPECT_TRUE(panel_.Form()->IsMarked("target_rms"));
  EXPECT_EQ(panel_.ResetButton()->text(),
            QStringLiteral("Reset to Encoder Defaults"));
  panel_.Reset();
  EXPECT_EQ(project_.Settings().default_encoder, EncoderOptions{});
}

// The preprocessing panel holds the form's preprocessing section, with a
// scope chooser that always matches the panel's.
TEST_F(EncoderPanelTest, PreprocessPanelSharesTheScope) {
  QWidget* preprocess = panel_.PreprocessPanel();
  EXPECT_TRUE(preprocess->isAncestorOf(panel_.Form()->PreprocessSection()));
  EXPECT_TRUE(preprocess->isAncestorOf(panel_.PreprocessScopeBox()));
  EXPECT_FALSE(preprocess->isAncestorOf(panel_.Form()->EncoderSection()));

  panel_.PreprocessScopeBox()->setCurrentIndex(1);
  EXPECT_EQ(panel_.CurrentScope(), EncoderPanel::Scope::kProjectDefaults);
  EXPECT_EQ(panel_.ScopeBox()->currentIndex(), 1);
  panel_.ScopeBox()->setCurrentIndex(0);
  EXPECT_EQ(panel_.CurrentScope(), EncoderPanel::Scope::kPhrase);
  EXPECT_EQ(panel_.PreprocessScopeBox()->currentIndex(), 0);
}

// Moved elsewhere, as the main window moves it to its own tab, the
// preprocessing panel still edits the phrase's settings.
TEST_F(EncoderPanelTest, PreprocessPanelWorksAwayFromThePanel) {
  QWidget elsewhere;
  elsewhere.setLayout(new QVBoxLayout);
  elsewhere.layout()->addWidget(panel_.PreprocessPanel());
  EXPECT_FALSE(panel_.isAncestorOf(panel_.PreprocessPanel()));
  Show(1);
  QCheckBox* gate = panel_.Form()->OnBox("noise_gate_db");
  ASSERT_NE(gate, nullptr);
  EXPECT_TRUE(elsewhere.isAncestorOf(gate));
  gate->setChecked(true);
  EXPECT_TRUE(panel_.HasPendingChange());
  EXPECT_EQ(panel_.ShownPreprocessing().noise_gate_db, 30.0);
}

TEST_F(EncoderPanelTest, LockedPhrasesShowTheirStatisticsOnly) {
  Show(3);
  EXPECT_FALSE(panel_.Form()->IsEditable());
  EXPECT_FALSE(panel_.ReEncodeButton()->isEnabled());
  EXPECT_FALSE(panel_.OptimiseButton()->isEnabled());
  EXPECT_TRUE(panel_.StateLabel()->text().startsWith(QStringLiteral("Locked")));
  EXPECT_TRUE(panel_.FramesLabel()->text().contains(QStringLiteral("frames")));
}

TEST_F(EncoderPanelTest, OptimiseButtonAsksForThePhraseShown) {
  Show(1);
  core::project::PhraseId asked;
  QObject::connect(&panel_, &EncoderPanel::OptimiseRequested,
                   [&asked](const core::project::PhraseId& id) { asked = id; });
  panel_.OptimiseButton()->click();
  EXPECT_EQ(asked, TestId(1));
}

TEST_F(EncoderPanelTest, NothingShownDisablesThePhrasesControls) {
  panel_.ShowPhrase(project_, {});
  EXPECT_FALSE(panel_.ReEncodeButton()->isEnabled());
  EXPECT_FALSE(panel_.Form()->IsEditable());
  EXPECT_EQ(panel_.StateLabel()->text(),
            QStringLiteral("Select one phrase to see its encoding"));
}

// A refused change: the edits go to a mocked editor.
TEST(EncoderPanelRefusalTest,
     RefusedChangeIsReportedAndTheFormShowsTheSettings) {
  const core::project::Project project =
      ProjectWith({EncodedPhrase(1, "HELLO", 100)});
  StrictMock<testing::MockProjectEditor> editor;
  QUndoStack stack;
  ProjectEdits edits(editor, stack);
  TestPlayback playback;
  TestEncoding encoding;
  core::InlineTaskRunner runner;
  EncoderPanel panel(edits, encoding.Tools(), playback.synth, runner);
  EXPECT_CALL(editor, SetEncoding(TestId(1), _))
      .WillOnce(Return(core::ResourceExhaustedError("over the budget")));
  QString failure;
  QObject::connect(&panel, &EncoderPanel::Failed,
                   [&failure](const QString&, const QString& message) {
                     failure = message;
                   });
  panel.ShowPhrase(project, TestId(1));
  QCoreApplication::processEvents();
  OptionSlider* rms = panel.Form()->Slider("target_rms");
  rms->MoveTo(30.0);
  panel.ReEncode();
  QCoreApplication::processEvents();
  EXPECT_EQ(failure, QStringLiteral("over the budget"));
  EXPECT_DOUBLE_EQ(rms->Value(), 20.0);
  EXPECT_EQ(stack.count(), 0);
}

}  // namespace
}  // namespace speak5220::app
