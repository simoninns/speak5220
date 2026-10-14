/*
 * File:        main_window_test.cpp
 * Module:      main_window tests
 * Purpose:     Unit tests for the application's top-level window, with
 *              mocked dialogs and no files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/main_window/main_window.h"

#include <QAction>
#include <QCheckBox>
#include <QComboBox>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include <QObject>
#include <QPushButton>
#include <QScrollArea>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <QToolButton>
#include <QUndoStack>
#include <QWidget>
#include <algorithm>
#include <optional>
#include <utility>
#include <vector>

#include "core/dsp/output_filter.h"
#include "core/preprocess/preprocess.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/synth/chip_clock.h"
#include "core/synth/output_pin.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/encoder_panel/encoder_panel.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/encoder_panel/quality_rating_view.h"
#include "gui/export/export_dialog.h"
#include "gui/main_window/dialogs.h"
#include "gui/optimise/optimise_dialog.h"
#include "gui/phrase_editor/phrase_details.h"
#include "gui/phrase_list/phrase_list_panel.h"
#include "gui/support/gui_mocks.h"
#include "gui/theme/colour_scheme.h"
#include "gui/waveform/phrase_tab.h"
#include "gui/waveform/phrase_waveforms.h"
#include "gui/waveform/waveform_pane.h"
#include "gui/waveform/waveform_view.h"
#include "support/project_builder.h"

namespace speak5220::app {
namespace {

using core::project::PhraseId;
using ::testing::_;
using testing::AudioPhrase;
using testing::EncodedPhrase;
using testing::FakeRecentFilesStore;
using testing::FakeViewPreferences;
using testing::MockDialogs;
using ::testing::Return;
using ::testing::StrictMock;
using testing::TestId;
using testing::TestPlayback;

class MainWindowTest : public ::testing::Test {
 protected:
  // Adds phrases as one undoable step, as an import would.
  void AddPhrases(std::vector<core::project::Phrase> phrases) {
    std::vector<core::project::PlacedPhrase> placed;
    placed.reserve(phrases.size());
    for (core::project::Phrase& phrase : phrases) {
      placed.push_back({.index = static_cast<int>(placed.size()),
                        .phrase = std::move(phrase)});
    }
    ASSERT_TRUE(window_.Document().Edits().InsertPhrases(placed).IsOk());
  }

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

TEST_F(MainWindowTest, NewProjectIsUntitledAndUnmodified) {
  EXPECT_EQ(window_.windowTitle(), QStringLiteral("Untitled[*] - Speak5220"));
  EXPECT_FALSE(window_.isWindowModified());
  EXPECT_EQ(window_.Model().PhraseCount(), 0);
  EXPECT_EQ(window_.StatusText(), QStringLiteral("No PHROMs in use"));
}

TEST_F(MainWindowTest, EditMarksTheProjectModifiedAndUndoClearsIt) {
  AddPhrases({EncodedPhrase(1, "HELLO", 1000)});
  EXPECT_TRUE(window_.isWindowModified());
  EXPECT_EQ(window_.Model().PhraseCount(), 1);
  EXPECT_EQ(window_.StatusText(),
            QStringLiteral("1.2 KB · 1 PHROM of 64 · 14.8 KB free "
                           "in PHROM 1"));
  window_.Document().UndoStack().undo();
  EXPECT_FALSE(window_.isWindowModified());
  EXPECT_EQ(window_.Model().PhraseCount(), 0);
}

TEST_F(MainWindowTest, SelectingOnePhraseShowsItsDetails) {
  AddPhrases({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(2)});
  EXPECT_EQ(window_.Details()->NameField()->text(), QStringLiteral("TWO"));
  window_.Panel()->Select({TestId(1), TestId(2)});
  EXPECT_FALSE(window_.Details()->NameField()->isEnabled());
}

TEST_F(MainWindowTest, SelectingAPhraseShowsItsEncoding) {
  AddPhrases({EncodedPhrase(1, "ONE", 100), AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(1)});
  QCoreApplication::processEvents();
  EXPECT_TRUE(window_.Encoder()->FramesLabel()->text().contains(
      QStringLiteral("frames")));
  EXPECT_TRUE(window_.Encoder()->ReEncodeButton()->isEnabled());
  window_.Panel()->Select({TestId(1), TestId(2)});
  EXPECT_FALSE(window_.Encoder()->ReEncodeButton()->isEnabled());
}

TEST_F(MainWindowTest, RemembersWhetherTheAdvancedSettingsAreShown) {
  EXPECT_FALSE(window_.Encoder()->Form()->AdvancedShown());
  window_.Encoder()->Form()->AdvancedButton()->click();
  EXPECT_TRUE(preferences_.advanced_shown);

  FakeViewPreferences shown;
  shown.advanced_shown = true;
  MainWindow window(dialogs_, recent_, shown, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_TRUE(window.Encoder()->Form()->AdvancedShown());
}

TEST_F(MainWindowTest, OutputFilterMenuSetsTheFilterAndRemembersIt) {
  using core::dsp::OutputFilter;
  EXPECT_TRUE(window_.OutputFilterAction(OutputFilter::kNone)->isChecked());
  window_.OutputFilterAction(OutputFilter::kBbcMicro)->trigger();
  EXPECT_TRUE(window_.OutputFilterAction(OutputFilter::kBbcMicro)->isChecked());
  EXPECT_FALSE(window_.OutputFilterAction(OutputFilter::kNone)->isChecked());
  EXPECT_EQ(playback_.player.Filter(), OutputFilter::kBbcMicro);
  EXPECT_EQ(window_.Waveforms()->Filter(), OutputFilter::kBbcMicro);
  EXPECT_EQ(preferences_.filter, OutputFilter::kBbcMicro);
}

TEST_F(MainWindowTest, ChipClockMenuSetsTheClockAndRemembersIt) {
  EXPECT_TRUE(
      window_.ChipClockAction(core::synth::kNominalClockHz)->isChecked());
  window_.ChipClockAction(680000)->trigger();
  EXPECT_TRUE(window_.ChipClockAction(680000)->isChecked());
  EXPECT_FALSE(
      window_.ChipClockAction(core::synth::kNominalClockHz)->isChecked());
  EXPECT_EQ(playback_.player.ChipClock(), 680000);
  EXPECT_EQ(preferences_.clock_hz, 680000);
  EXPECT_EQ(window_.ChipClockAction(123), nullptr);
}

TEST_F(MainWindowTest, ChipClockStartsAsLastChosen) {
  FakeViewPreferences chosen;
  chosen.clock_hz = 600000;
  MainWindow window(dialogs_, recent_, chosen, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_TRUE(window.ChipClockAction(600000)->isChecked());
  EXPECT_EQ(playback_.player.ChipClock(), 600000);
}

TEST_F(MainWindowTest, OutputFilterStartsAsLastChosen) {
  using core::dsp::OutputFilter;
  FakeViewPreferences chosen;
  chosen.filter = OutputFilter::kBbcMicro;
  MainWindow window(dialogs_, recent_, chosen, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_TRUE(window.OutputFilterAction(OutputFilter::kBbcMicro)->isChecked());
  EXPECT_EQ(window.Waveforms()->Filter(), OutputFilter::kBbcMicro);
}

TEST_F(MainWindowTest, ChipOutputMenuSetsThePinAndRemembersIt) {
  using core::synth::OutputPin;
  EXPECT_TRUE(window_.OutputPinAction(OutputPin::kSpeaker)->isChecked());
  EXPECT_EQ(window_.Waveforms()->Pin(), OutputPin::kSpeaker);
  window_.OutputPinAction(OutputPin::kIo)->trigger();
  EXPECT_TRUE(window_.OutputPinAction(OutputPin::kIo)->isChecked());
  EXPECT_FALSE(window_.OutputPinAction(OutputPin::kSpeaker)->isChecked());
  EXPECT_EQ(window_.Waveforms()->Pin(), OutputPin::kIo);
  EXPECT_EQ(window_.OptimiseWindow()->Pin(), OutputPin::kIo);
  EXPECT_EQ(preferences_.pin, OutputPin::kIo);
}

TEST_F(MainWindowTest, ChipOutputStartsAsLastChosen) {
  using core::synth::OutputPin;
  FakeViewPreferences chosen;
  chosen.pin = OutputPin::kIo;
  MainWindow window(dialogs_, recent_, chosen, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_TRUE(window.OutputPinAction(OutputPin::kIo)->isChecked());
  EXPECT_EQ(window.Waveforms()->Pin(), OutputPin::kIo);
}

TEST_F(MainWindowTest, ThemeMenuChoosesTheThemeAndRemembersIt) {
  EXPECT_TRUE(window_.ThemeAction(ThemeChoice::kAuto)->isChecked());
  std::vector<ThemeChoice> chosen;
  QObject::connect(&window_, &MainWindow::ThemeChanged,
                   [&chosen](ThemeChoice choice) { chosen.push_back(choice); });
  window_.ThemeAction(ThemeChoice::kDark)->trigger();
  EXPECT_TRUE(window_.ThemeAction(ThemeChoice::kDark)->isChecked());
  EXPECT_FALSE(window_.ThemeAction(ThemeChoice::kAuto)->isChecked());
  EXPECT_EQ(preferences_.theme, ThemeChoice::kDark);
  window_.ThemeAction(ThemeChoice::kLight)->trigger();
  EXPECT_EQ(chosen, (std::vector<ThemeChoice>{ThemeChoice::kDark,
                                              ThemeChoice::kLight}));
  EXPECT_EQ(preferences_.theme, ThemeChoice::kLight);
}

TEST_F(MainWindowTest, ThemeStartsAsLastChosen) {
  FakeViewPreferences chosen;
  chosen.theme = ThemeChoice::kDark;
  MainWindow window(dialogs_, recent_, chosen, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_TRUE(window.ThemeAction(ThemeChoice::kDark)->isChecked());
}

TEST_F(MainWindowTest, TurningTheNoiseGateOnShadesThePreprocessedAudio) {
  // Loud, then 60 dB quieter, a tenth of a second each.
  core::project::Phrase breath = AudioPhrase(1, "BREATH");
  breath.audio.assign(1600, 0.5F);
  std::fill(breath.audio.begin() + 800, breath.audio.end(), 0.0005F);
  AddPhrases({breath, AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(1)});
  WaveformView* preprocessed = window_.Waveforms()->PreprocessedPane()->View();
  EXPECT_TRUE(preprocessed->Shading().empty());
  window_.Encoder()->Form()->OnBox("noise_gate_db")->setChecked(true);
  EXPECT_EQ(window_.Waveforms()->Preprocessing().noise_gate_db, 30.0);
  EXPECT_FALSE(preprocessed->Shading().empty());
  EXPECT_TRUE(window_.Waveforms()->OriginalPane()->View()->Shading().empty());
  // Selecting several phrases shows none, and so no noise gate.
  window_.Panel()->Select({TestId(1), TestId(2)});
  EXPECT_EQ(window_.Waveforms()->Preprocessing().noise_gate_db, std::nullopt);
  EXPECT_TRUE(preprocessed->Shading().empty());
}

// Each tab's settings area holds its settings: the preprocessing settings
// on the Preprocess tab, the encoder panel on the Encode tab.
TEST_F(MainWindowTest, TabsHoldTheirOwnSettings) {
  const PhraseWaveforms* waveforms = window_.Waveforms();
  const EncoderPanel* encoder = window_.Encoder();
  QWidget* preprocess = waveforms->SettingsArea(PhraseTab::kPreprocess);
  QWidget* encode = waveforms->SettingsArea(PhraseTab::kEncode);
  EXPECT_TRUE(preprocess->isAncestorOf(encoder->PreprocessPanel()));
  EXPECT_TRUE(preprocess->isAncestorOf(encoder->Form()->PreprocessSection()));
  EXPECT_TRUE(preprocess->isAncestorOf(encoder->PreprocessScopeBox()));
  EXPECT_FALSE(preprocess->isAncestorOf(encoder->Form()->EncoderSection()));
  EXPECT_TRUE(encode->isAncestorOf(encoder->Form()->EncoderSection()));
  EXPECT_TRUE(encode->isAncestorOf(encoder->Quality()));
  EXPECT_FALSE(encode->isAncestorOf(encoder->PreprocessPanel()));
  EXPECT_FALSE(waveforms->SettingsArea(PhraseTab::kOriginal)
                   ->isAncestorOf(encoder->PreprocessPanel()));
}

// The tab shown is remembered between sessions.
TEST_F(MainWindowTest, RemembersTheTabShown) {
  EXPECT_EQ(window_.Waveforms()->CurrentTab(), PhraseTab::kOriginal);
  window_.Waveforms()->Tabs()->setCurrentIndex(1);
  EXPECT_EQ(preferences_.phrase_tab, PhraseTab::kPreprocess);
  window_.Waveforms()->SetCurrentTab(PhraseTab::kEncode);
  EXPECT_EQ(preferences_.phrase_tab, PhraseTab::kEncode);

  FakeViewPreferences shown;
  shown.phrase_tab = PhraseTab::kEncode;
  MainWindow window(dialogs_, recent_, shown, runner_, playback_.synth,
                    playback_.player, encoding_.Tools());
  EXPECT_EQ(window.Waveforms()->CurrentTab(), PhraseTab::kEncode);
}

// The preprocessing the encoder panel shows is what the waveform shows.
TEST_F(MainWindowTest, ThePresetsPreprocessingIsShownOnTheWaveform) {
  AddPhrases({AudioPhrase(1, "ONE")});
  window_.Panel()->Select({TestId(1)});
  emit window_.Encoder()->Form()->PresetBox()->activated(1);
  EXPECT_EQ(window_.Waveforms()->Preprocessing(),
            core::preprocess::PreprocessPresets()[1].options);
}

TEST_F(MainWindowTest, ReEncodeEncodesTheSelectionAsOneStep) {
  AddPhrases({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO"),
              testing::LockedPhrase(3, "THREE", 50)});
  window_.Panel()->Select({TestId(1), TestId(2), TestId(3)});
  const int steps = window_.Document().UndoStack().count();
  window_.ReEncodeSelected();
  QCoreApplication::processEvents();
  EXPECT_EQ(window_.EncodesRunning(), 0);
  const core::project::Project& project = window_.Document().Project();
  ASSERT_TRUE(project.FindPhrase(TestId(1))->encoding.has_value());
  ASSERT_TRUE(project.FindPhrase(TestId(2))->encoding.has_value());
  ASSERT_TRUE(project.FindPhrase(TestId(2))->encoding->quality.has_value());
  EXPECT_NEAR(project.FindPhrase(TestId(2))->encoding->quality->score,
              testing::TestEncoding::kScore, 1e-9);
  EXPECT_EQ(window_.Document().UndoStack().count(), steps + 1);
  EXPECT_EQ(window_.Document().UndoStack().undoText(),
            QStringLiteral("Re-encode 2 Phrases"));
}

TEST_F(MainWindowTest, ReEncodeIsOfferedOnlyForStaleOrUnencodedPhrases) {
  QAction* re_encode = nullptr;
  for (QAction* action : window_.findChildren<QAction*>()) {
    if (action->text() == QStringLiteral("Re-&encode")) {
      re_encode = action;
    }
  }
  ASSERT_NE(re_encode, nullptr);
  AddPhrases({AudioPhrase(1, "ONE"), testing::LockedPhrase(2, "TWO", 50)});
  window_.Panel()->Select({TestId(2)});
  EXPECT_FALSE(re_encode->isEnabled());
  window_.Panel()->Select({TestId(1)});
  EXPECT_TRUE(re_encode->isEnabled());
  window_.ReEncodeSelected();
  QCoreApplication::processEvents();
  EXPECT_FALSE(re_encode->isEnabled());
  const int steps = window_.Document().UndoStack().count();
  window_.ReEncodeSelected();
  QCoreApplication::processEvents();
  EXPECT_EQ(window_.Document().UndoStack().count(), steps);
}

TEST_F(MainWindowTest, OptimiseIsOfferedForPhrasesThatCanBeEncoded) {
  AddPhrases({AudioPhrase(1, "ONE"), testing::LockedPhrase(2, "TWO", 50)});
  window_.Panel()->Select({TestId(2)});
  EXPECT_FALSE(window_.Encoder()->OptimiseButton()->isEnabled());
  window_.Panel()->Select({TestId(1)});
  EXPECT_TRUE(window_.Encoder()->OptimiseButton()->isEnabled());
  window_.Encoder()->OptimiseButton()->click();
  EXPECT_TRUE(window_.OptimiseWindow()->isVisible());
  EXPECT_EQ(window_.OptimiseWindow()->windowTitle(),
            QStringLiteral("Optimise ONE"));
  window_.OptimiseWindow()->reject();
}

TEST_F(MainWindowTest, SelectionFollowsAPhraseThatMoves) {
  AddPhrases({AudioPhrase(1, "CHARLIE"), AudioPhrase(2, "ALPHA"),
              AudioPhrase(3, "BRAVO")});
  window_.Panel()->Select({TestId(1)});
  ASSERT_TRUE(window_.Model().Sort());
  EXPECT_EQ(window_.Panel()->SelectedIds(), std::vector<PhraseId>{TestId(1)});
  EXPECT_EQ(window_.Panel()->SelectedRows(), QList<int>{2});
}

TEST_F(MainWindowTest, RemoveAsksFirst) {
  AddPhrases({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(2)});
  QAction* remove = nullptr;
  for (QAction* action : window_.findChildren<QAction*>()) {
    if (action->text() == QStringLiteral("&Remove")) {
      remove = action;
    }
  }
  ASSERT_NE(remove, nullptr);
  ASSERT_TRUE(remove->isEnabled());
  EXPECT_CALL(dialogs_, ConfirmRemove(QStringList{QStringLiteral("TWO")}))
      .WillOnce(Return(false))
      .WillOnce(Return(true));
  remove->trigger();
  EXPECT_EQ(window_.Model().PhraseCount(), 2);
  remove->trigger();
  EXPECT_EQ(window_.Model().PhraseCount(), 1);
}

TEST_F(MainWindowTest, ExportOffersTheSelectionAndCanBeCancelled) {
  AddPhrases({AudioPhrase(1, "ONE"), AudioPhrase(2, "TWO")});
  window_.Panel()->Select({TestId(2)});
  const int steps = window_.Document().UndoStack().count();
  EXPECT_CALL(dialogs_, AskExport(_))
      .WillOnce(
          [this](const ExportStart& start) -> std::optional<ExportRequest> {
            EXPECT_EQ(start.project, &window_.Document().Project());
            EXPECT_EQ(start.selected, QStringList{QStringLiteral("TWO")});
            EXPECT_EQ(start.base, QStringLiteral("Untitled"));
            return std::nullopt;
          });
  window_.ExportProject();
  EXPECT_EQ(window_.ExportsRunning(), 0);
  EXPECT_EQ(window_.Document().UndoStack().count(), steps);
}

TEST_F(MainWindowTest, ExportRefusesStalePhrases) {
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, AskExport(_))
      .WillOnce(Return(
          ExportRequest{.preset = {.name = "words", .format = "c-header-phrom"},
                        .directory = QStringLiteral("/out")}));
  EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Export"), _))
      .WillOnce([](const QString&, const QString& message) {
        EXPECT_TRUE(message.contains(QStringLiteral("ONE is not encoded")));
      });
  window_.ExportProject();
  EXPECT_EQ(window_.ExportsRunning(), 0);
}

TEST_F(MainWindowTest, ClosingWithUnsavedChangesCanBeCancelled) {
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, AskSaveChanges(QStringLiteral("Untitled")))
      .WillOnce(Return(SaveChoice::kCancel));
  EXPECT_FALSE(window_.close());
}

TEST_F(MainWindowTest, ClosingWithUnsavedChangesCanDiscardThem) {
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, AskSaveChanges(_))
      .WillOnce(Return(SaveChoice::kDiscard));
  EXPECT_TRUE(window_.close());
}

TEST_F(MainWindowTest, ClosingWhenSaveIsCancelledKeepsTheWindowOpen) {
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, AskSaveChanges(_)).WillOnce(Return(SaveChoice::kSave));
  EXPECT_CALL(dialogs_, SaveProjectPath(_)).WillOnce(Return(std::nullopt));
  EXPECT_FALSE(window_.close());
  EXPECT_TRUE(window_.isWindowModified());
}

TEST_F(MainWindowTest, ClosingAnUnmodifiedProjectAsksNothing) {
  EXPECT_TRUE(window_.close());
}

TEST_F(MainWindowTest, FailedListEditIsShown) {
  // A rename to a taken name, made through the details, is shown there; a
  // failed edit from the list goes to a dialog.
  AddPhrases({AudioPhrase(1, "ONE")});
  EXPECT_CALL(dialogs_, ShowError(QStringLiteral("Edit Phrases"), _));
  emit window_.Model().EditFailed(QStringLiteral("cannot move"));
}

}  // namespace
}  // namespace speak5220::app
