/*
 * File:        optimise_dialog.h
 * Module:      optimise
 * Purpose:     The Optimise window: limits, progress, the best so far
 *              against the start, the candidate history, and accept or
 *              revert
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_OPTIMISE_OPTIMISE_DIALOG_H_
#define SPEAK5220_GUI_OPTIMISE_OPTIMISE_DIALOG_H_

#include <QDialog>
#include <QString>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/playback/player.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QEvent;
class QLabel;
class QProgressBar;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QToolButton;
class QWidget;

namespace speak5220::app {

class OptimiseSession;
class QualityRatingView;

// Plan §3.3 and Phase 7, "Optimise in the GUI". Prepare() shows the
// limits for the phrases chosen: an optional byte limit per phrase, and an
// optional weight that trades quality for bit rate (the overall score less
// the weight per kbit/s). Start runs the session; while it runs the window
// shows the progress, the phrase's best so far on the quality rating with
// a mark for its starting score, and the best settings' changes; every
// candidate can be shown in a table. When it finishes, Accept keeps the results
// (one undoable step) and Revert keeps the current settings. Cancel, or closing
// the window, stops the search and changes nothing.
//
// Before accepting, each changed phrase can be heard both ways: Original
// plays the frames it has now (what Revert keeps), Optimised the frames
// found (what Accept keeps). Both are spoken through the emulator on the
// runner, once each, and played at the recording's level, as the encoded
// waveform pane plays them, so they compare like with like. The buttons
// play and pause like the waveform panes' (kBeforeOptimise and
// kOptimised); playing one stops the other.
//
// GUI thread only; the session, player, synth and runner must outlive the
// dialog, whose destructor waits for the runner.
class OptimiseDialog : public QDialog {
  Q_OBJECT

 public:
  OptimiseDialog(OptimiseSession& session, Player& player,
                 core::synth::ISynth& synth, core::ITaskRunner& runner,
                 QWidget* parent = nullptr);
  ~OptimiseDialog() override;
  OptimiseDialog(const OptimiseDialog&) = delete;
  OptimiseDialog& operator=(const OptimiseDialog&) = delete;

  // Readies the window for the phrases of `project` with `ids`.
  void Prepare(const core::project::Project& project,
               std::vector<core::project::PhraseId> ids);
  // Starts the search with the limits shown.
  void StartSearch();

  QCheckBox* LimitBox() const { return limit_box_; }
  QSpinBox* LimitSpin() const { return limit_; }
  QCheckBox* WeightBox() const { return weight_box_; }
  QDoubleSpinBox* WeightSpin() const { return weight_; }
  QPushButton* StartButton() const { return start_; }
  QPushButton* CancelButton() const { return cancel_; }
  QPushButton* AcceptButton() const { return accept_; }
  QPushButton* RevertButton() const { return revert_; }
  QPushButton* CloseButton() const { return close_; }
  QProgressBar* Progress() const { return progress_; }
  QualityRatingView* Quality() const { return quality_; }
  QLabel* StatusLabel() const { return status_; }
  QLabel* BestLabel() const { return best_; }
  QCheckBox* HistoryBox() const { return show_history_; }
  QTableWidget* History() const { return history_; }
  // The phrase to listen to (hidden for one), and its two play buttons.
  QComboBox* ListenPhrase() const { return listen_phrase_; }
  QToolButton* PlayOriginalButton() const { return play_original_; }
  QToolButton* PlayOptimisedButton() const { return play_optimised_; }
  // True while `source`'s button shows pause.
  bool ShowsPause(PlaybackSource source) const;
  // Speakings started and not yet finished.
  int RendersRunning() const { return renders_running_; }

  // Plays the listened phrase's frames before Optimise (kBeforeOptimise)
  // or the optimised frames (kOptimised) from the beginning, or pauses or
  // resumes them if already playing or paused.
  void PlayPause(PlaybackSource source);

  // The chip's pin the two encodings are heard from, as the encoded
  // waveform pane has it; SPEAKER at first.
  void SetOutputPin(core::synth::OutputPin pin) { pin_ = pin; }
  core::synth::OutputPin Pin() const { return pin_; }

 signals:
  // Accepting the results failed.
  void Failed(const QString& title, const QString& message);

 public slots:
  // Cancels a running search and discards the results.
  void reject() override;

 protected:
  // Redraws the play/pause icons in the new palette's colours.
  void changeEvent(QEvent* event) override;

 private:
  enum class Stage { kReady, kRunning, kDone, kNothing };

  // A phrase's two encodings, once spoken.
  struct Comparison {
    // False if the phrase had no frames before Optimise.
    bool had_frames = false;
    std::optional<core::quality::EncodedAudio> original;
    std::optional<core::quality::EncodedAudio> optimised;
    // Why one could not be spoken.
    QString problem;
  };

  void SetStage(Stage stage);
  void OnProgress();
  void OnHistoryAdded(int first);
  void OnFinished();
  void OnAccept();
  // Offers the changed phrases for listening, and speaks the first.
  void PrepareListening();
  // Speaks the listened phrase's two encodings, if not done yet.
  void SpeakListened();
  void Speak(int phrase, bool optimised, std::vector<float> audio,
             std::vector<core::lpc::Frame> frames);
  void OnSpoken(int generation, int phrase, bool optimised,
                core::StatusOr<core::quality::EncodedAudio> result);
  // The phrase listened to, an index into the session's phrases, or -1.
  int ListenedPhrase() const;
  void UpdateListenButtons();
  // Stops the player if it plays this window's audio.
  void StopListening();

  OptimiseSession* session_;
  std::optional<core::project::Project> project_;
  std::vector<core::project::PhraseId> ids_;
  QCheckBox* limit_box_;
  QSpinBox* limit_;
  QCheckBox* weight_box_;
  QDoubleSpinBox* weight_;
  QLabel* status_;
  QProgressBar* progress_;
  QualityRatingView* quality_;
  QLabel* best_;
  QCheckBox* show_history_;
  QTableWidget* history_;
  QPushButton* start_;
  QPushButton* cancel_;
  QPushButton* accept_;
  QPushButton* revert_;
  QPushButton* close_;
  QWidget* listen_row_;
  QComboBox* listen_phrase_;
  QToolButton* play_original_;
  QToolButton* play_optimised_;
  QLabel* listen_note_;
  Player* player_;
  core::synth::ISynth* synth_;
  core::ITaskRunner* runner_;
  // By phrase, an index into the session's phrases.
  std::map<int, Comparison> comparisons_;
  // Moves on with each search, so a speaking from an earlier one is
  // dropped.
  int generation_ = 0;
  int renders_running_ = 0;
  core::synth::OutputPin pin_ = core::synth::OutputPin::kSpeaker;
  bool original_shows_pause_ = false;
  bool optimised_shows_pause_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_OPTIMISE_OPTIMISE_DIALOG_H_
