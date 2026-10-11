/*
 * File:        optimise_session_test.cpp
 * Module:      optimise tests
 * Purpose:     Unit tests of Optimise in the GUI, the session and its
 *              window, with mocked encoding tools on inline runners
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/optimise/optimise_session.h"

#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QTableWidget>
#include <QToolButton>
#include <QUndoStack>
#include <span>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/optimiser/optimiser.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/quality_scale_view.h"
#include "gui/optimise/optimise_dialog.h"
#include "gui/playback/player.h"
#include "gui/support/gui_mocks.h"
#include "gui/undo/project_edits.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using core::encoder::EncoderOptions;
using ::testing::_;
using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::LockedPhrase;
using testing::ProjectWith;
using ::testing::SizeIs;
using testing::TestEncoding;
using testing::TestId;

// A short search keeps the tests quick.
constexpr core::optimiser::SearchPlan kPlan = {.max_evaluations = 40,
                                               .max_rounds = 2};

class OptimiseSessionTest : public ::testing::Test {
 protected:
  OptimiseSessionTest() {
    // Louder scores better, so the best is never the start; the encoder
    // remembers the options the metric then scores (inline runners).
    ON_CALL(encoding_.encoder, Encode(_, _, _))
        .WillByDefault([this](std::span<const double>, int,
                              const EncoderOptions& options)
                           -> core::StatusOr<core::encoder::EncodeResult> {
          last_ = options;
          return core::encoder::EncodeResult{
              .frames = testing::FramesPackingTo(20), .details = std::nullopt};
        });
    ON_CALL(encoding_.quality, Measure(_, _))
        .WillByDefault(
            [this](
                std::span<const double>,
                const core::encoder::EncodeResult&) -> core::StatusOr<double> {
              if (cancel_after_ > 0 && ++measured_ == cancel_after_) {
                session_.Cancel();
              }
              return 1.5 + (last_.target_rms / 40.0);
            });
  }

  core::Status Start(const std::vector<core::project::PhraseId>& ids) {
    const core::Status status = session_.Start(project_, ids, {}, kPlan);
    QCoreApplication::processEvents();
    return status;
  }

  const core::project::Phrase& Phrase(int n) const {
    return *project_.FindPhrase(TestId(n));
  }

  core::project::Project project_ =
      ProjectWith({EncodedPhrase(1, "HELLO", 100), AudioPhrase(2, "WORLD"),
                   LockedPhrase(3, "LOCKED", 50)});
  QUndoStack stack_;
  ProjectEdits edits_{project_, stack_};
  TestEncoding encoding_;
  core::InlineTaskRunner workers_;
  OptimiseSession session_{edits_, encoding_.Tools(), workers_};
  EncoderOptions last_;
  int cancel_after_ = 0;
  int measured_ = 0;
};

TEST_F(OptimiseSessionTest, AcceptKeepsTheBestAsOneUndoableStep) {
  const core::project::Project before = project_;
  ASSERT_TRUE(Start({TestId(1)}).IsOk());
  EXPECT_FALSE(session_.IsRunning());
  ASSERT_EQ(session_.Phrases().size(), 1U);
  const PhraseOptimisation& result = session_.Phrases().front();
  ASSERT_TRUE(result.best.has_value());
  EXPECT_GT(result.best->options.target_rms, 20.0);
  EXPECT_EQ(result.anchor, TestEncoding::kAnchor);
  EXPECT_EQ(session_.ChangeCount(), 1);
  // Nothing changes before Accept.
  EXPECT_EQ(project_, before);

  ASSERT_TRUE(session_.Accept().IsOk());
  ASSERT_TRUE(Phrase(1).encoder.contains("target_rms"));
  ASSERT_TRUE(Phrase(1).encoding.has_value());
  EXPECT_EQ(Phrase(1).encoding->pesq, result.best->score);
  EXPECT_EQ(Phrase(1).encoding->anchor, TestEncoding::kAnchor);
  ASSERT_EQ(stack_.count(), 1);
  EXPECT_EQ(stack_.text(0), QStringLiteral("Optimise HELLO"));
  stack_.undo();
  EXPECT_EQ(project_, before);
  // Accepted once only.
  EXPECT_FALSE(session_.Accept().IsOk());
}

TEST_F(OptimiseSessionTest, KeepsEveryCandidateInTheHistory) {
  ASSERT_TRUE(Start({TestId(1)}).IsOk());
  ASSERT_EQ(session_.History().size(),
            static_cast<std::size_t>(kPlan.max_evaluations));
  for (std::size_t i = 0; i < session_.History().size(); ++i) {
    EXPECT_EQ(session_.History()[i].phrase, 0);
    EXPECT_EQ(session_.History()[i].candidate.index, static_cast<int>(i));
  }
  EXPECT_EQ(session_.Evaluated(), kPlan.max_evaluations);
}

TEST_F(OptimiseSessionTest, CancellingLeavesTheSettingsUnchanged) {
  const core::project::Project before = project_;
  cancel_after_ = 5;
  ASSERT_TRUE(Start({TestId(1), TestId(2)}).IsOk());
  EXPECT_FALSE(session_.IsRunning());
  EXPECT_TRUE(session_.WasCancelled());
  EXPECT_EQ(session_.ChangeCount(), 0);
  EXPECT_FALSE(session_.Accept().IsOk());
  EXPECT_EQ(project_, before);
  EXPECT_EQ(stack_.count(), 0);
}

TEST_F(OptimiseSessionTest, OptimiseAllChangesEachPhraseInOneStep) {
  ASSERT_TRUE(Start({TestId(2), TestId(1), TestId(3)}).IsOk());
  // The locked phrase is left out; the others run in list order.
  ASSERT_EQ(session_.Phrases().size(), 2U);
  EXPECT_EQ(session_.Phrases()[0].id, TestId(1));
  EXPECT_EQ(session_.Phrases()[1].id, TestId(2));
  ASSERT_TRUE(session_.Accept().IsOk());
  EXPECT_TRUE(Phrase(1).encoder.contains("target_rms"));
  EXPECT_TRUE(Phrase(2).encoder.contains("target_rms"));
  ASSERT_EQ(stack_.count(), 1);
  EXPECT_EQ(stack_.text(0), QStringLiteral("Optimise 2 Phrases"));
}

TEST_F(OptimiseSessionTest, RefusesPhrasesThatCannotBeEncoded) {
  EXPECT_EQ(Start({TestId(3)}).Code(), core::StatusCode::kFailedPrecondition);
  EXPECT_FALSE(session_.IsRunning());
}

TEST_F(OptimiseSessionTest, DiscardForgetsTheResults) {
  const core::project::Project before = project_;
  ASSERT_TRUE(Start({TestId(1)}).IsOk());
  session_.Discard();
  EXPECT_EQ(session_.ChangeCount(), 0);
  EXPECT_FALSE(session_.Accept().IsOk());
  EXPECT_EQ(project_, before);
}

class OptimiseDialogTest : public OptimiseSessionTest {
 protected:
  // Searches and lets the frames be spoken for listening.
  void Search() {
    dialog_.StartButton()->click();
    // The search's results, then the speakings they start.
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();
  }

  testing::TestPlayback playback_;
  core::InlineTaskRunner runner_;
  OptimiseDialog dialog_{session_, playback_.player, playback_.synth, runner_};
};

TEST_F(OptimiseDialogTest, StartsReadyWithTheLimitsOff) {
  dialog_.Prepare(project_, {TestId(1)});
  EXPECT_EQ(dialog_.windowTitle(), QStringLiteral("Optimise HELLO"));
  EXPECT_TRUE(dialog_.StartButton()->isVisibleTo(&dialog_));
  EXPECT_FALSE(dialog_.AcceptButton()->isVisibleTo(&dialog_));
  EXPECT_FALSE(dialog_.LimitBox()->isChecked());
  EXPECT_FALSE(dialog_.LimitSpin()->isEnabled());
  // The limit starts at the phrase's size now.
  EXPECT_EQ(dialog_.LimitSpin()->value(),
            *core::project::PackedSize(Phrase(1).encoding->frames));
}

TEST_F(OptimiseDialogTest, ShowsTheBestAgainstTheStartAndAccepts) {
  dialog_.Prepare(project_, {TestId(1)});
  dialog_.StartButton()->click();
  QCoreApplication::processEvents();
  EXPECT_TRUE(dialog_.AcceptButton()->isVisibleTo(&dialog_));
  EXPECT_TRUE(dialog_.RevertButton()->isVisibleTo(&dialog_));
  EXPECT_FALSE(dialog_.CancelButton()->isVisibleTo(&dialog_));
  ASSERT_TRUE(dialog_.Quality()->Bar()->Before().has_value());
  ASSERT_TRUE(dialog_.Quality()->Bar()->Position().has_value());
  EXPECT_GT(*dialog_.Quality()->Bar()->Position(),
            *dialog_.Quality()->Bar()->Before());
  EXPECT_TRUE(
      dialog_.BestLabel()->text().contains(QStringLiteral("Target RMS")));
  EXPECT_EQ(dialog_.History()->rowCount(),
            static_cast<int>(session_.History().size()));
  EXPECT_FALSE(dialog_.History()->isVisibleTo(&dialog_));
  dialog_.HistoryBox()->setChecked(true);
  EXPECT_TRUE(dialog_.History()->isVisibleTo(&dialog_));

  dialog_.AcceptButton()->click();
  EXPECT_TRUE(Phrase(1).encoder.contains("target_rms"));
  EXPECT_EQ(dialog_.result(), QDialog::Accepted);
}

TEST_F(OptimiseDialogTest, RevertKeepsTheCurrentSettings) {
  const core::project::Project before = project_;
  dialog_.Prepare(project_, {TestId(1)});
  dialog_.StartButton()->click();
  QCoreApplication::processEvents();
  dialog_.RevertButton()->click();
  EXPECT_EQ(project_, before);
  EXPECT_EQ(dialog_.result(), QDialog::Rejected);
}

TEST_F(OptimiseDialogTest, CancelledSearchOffersOnlyClose) {
  const core::project::Project before = project_;
  cancel_after_ = 3;
  dialog_.Prepare(project_, {TestId(1)});
  dialog_.StartButton()->click();
  QCoreApplication::processEvents();
  EXPECT_FALSE(dialog_.AcceptButton()->isVisibleTo(&dialog_));
  EXPECT_TRUE(dialog_.CloseButton()->isVisibleTo(&dialog_));
  EXPECT_TRUE(
      dialog_.StatusLabel()->text().startsWith(QStringLiteral("Cancelled")));
  EXPECT_EQ(project_, before);
}

TEST_F(OptimiseDialogTest, PassesTheByteLimitToTheSearch) {
  dialog_.Prepare(project_, {TestId(1)});
  dialog_.LimitBox()->setChecked(true);
  // Every candidate is 20 bytes: over a limit of 10.
  dialog_.LimitSpin()->setValue(10);
  dialog_.StartButton()->click();
  QCoreApplication::processEvents();
  EXPECT_EQ(session_.ChangeCount(), 0);
  EXPECT_TRUE(dialog_.StatusLabel()->text().startsWith(
      QStringLiteral("No better settings")));
  EXPECT_EQ(session_.History().front().candidate.outcome,
            core::optimiser::CandidateOutcome::kOverLimit);
}

TEST_F(OptimiseDialogTest, ListeningIsOfferedOnlyOnceSomethingIsFound) {
  dialog_.Prepare(project_, {TestId(1)});
  EXPECT_FALSE(dialog_.PlayOriginalButton()->isVisibleTo(&dialog_));
  Search();
  EXPECT_EQ(dialog_.RendersRunning(), 0);
  EXPECT_TRUE(dialog_.PlayOriginalButton()->isVisibleTo(&dialog_));
  EXPECT_TRUE(dialog_.PlayOriginalButton()->isEnabled());
  EXPECT_TRUE(dialog_.PlayOptimisedButton()->isEnabled());
  // One phrase: nothing to choose.
  EXPECT_FALSE(dialog_.ListenPhrase()->isVisibleTo(&dialog_));
}

TEST_F(OptimiseDialogTest, PlaysEitherEncodingAndSwitchesBetweenThem) {
  dialog_.Prepare(project_, {TestId(1)});
  Search();
  EXPECT_CALL(playback_.output,
              Start(SizeIs(testing::TestPlayback::kSpokenSamples), _))
      .Times(2);
  dialog_.PlayOptimisedButton()->click();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPlaying);
  EXPECT_EQ(playback_.player.Source(), PlaybackSource::kOptimised);
  EXPECT_TRUE(dialog_.ShowsPause(PlaybackSource::kOptimised));
  // Pressing it again pauses.
  dialog_.PlayOptimisedButton()->click();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kPaused);
  EXPECT_FALSE(dialog_.ShowsPause(PlaybackSource::kOptimised));
  // The other takes over, from the beginning.
  dialog_.PlayOriginalButton()->click();
  EXPECT_EQ(playback_.player.Source(), PlaybackSource::kBeforeOptimise);
  EXPECT_EQ(playback_.player.Position(), 0);
  EXPECT_TRUE(dialog_.ShowsPause(PlaybackSource::kBeforeOptimise));
  EXPECT_FALSE(dialog_.ShowsPause(PlaybackSource::kOptimised));
}

TEST_F(OptimiseDialogTest, ClosingStopsTheListening) {
  dialog_.Prepare(project_, {TestId(1)});
  Search();
  dialog_.PlayOptimisedButton()->click();
  dialog_.RevertButton()->click();
  EXPECT_EQ(playback_.player.State(), PlaybackState::kStopped);
}

TEST_F(OptimiseDialogTest, PhraseWithNoFramesBeforeHasOnlyTheOptimised) {
  dialog_.Prepare(project_, {TestId(1), TestId(2)});
  Search();
  ASSERT_EQ(dialog_.ListenPhrase()->count(), 2);
  EXPECT_TRUE(dialog_.ListenPhrase()->isVisibleTo(&dialog_));
  // Its frames are spoken when it is chosen.
  dialog_.ListenPhrase()->setCurrentIndex(1);
  EXPECT_EQ(dialog_.RendersRunning(), 1);
  QCoreApplication::processEvents();
  EXPECT_EQ(dialog_.ListenPhrase()->currentText(), QStringLiteral("WORLD"));
  EXPECT_FALSE(dialog_.PlayOriginalButton()->isEnabled());
  EXPECT_TRUE(dialog_.PlayOptimisedButton()->isEnabled());
}

}  // namespace
}  // namespace speak5220::app
