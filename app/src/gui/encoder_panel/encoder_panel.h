/*
 * File:        encoder_panel.h
 * Module:      encoder_panel
 * Purpose:     The phrase editor's encoder panel: statistics, the quality
 *              scale, the settings form, and re-encoding in the background
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_ENCODER_PANEL_ENCODER_PANEL_H_
#define SPEAK5220_GUI_ENCODER_PANEL_ENCODER_PANEL_H_

#include <QString>
#include <QWidget>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/preprocess/preprocess.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/encoder_panel/phrase_encoder.h"
#include "gui/undo/project_edits.h"

class QComboBox;
class QLabel;
class QPushButton;
class QTimer;

namespace speak5220::app {

class EncoderSettingsForm;
class QualityRatingView;

// Plan §5, "Encoder panel", and Phase 7: on the Encode tab, under the
// encoded waveform, the selected phrase's encoding.
//
// - Statistics (FramesText(), KindsText(), LevelText()) and the quality
//   (QualityRatingView): the rating, components and figures stored with
//   the encoding, or measured in the background for display when it has
//   none.
// - The settings form, for this phrase (over the project's defaults) or
//   for the project's defaults themselves, chosen above it. Its
//   preprocessing section is in the preprocessing panel
//   (PreprocessPanel()) with a scope chooser of its own, which always
//   matches the panel's: the main window puts it on the Preprocess tab.
// - A change of setting is applied after kDebounceMs without another
//   change (timed through IClock): for the phrase, it is encoded and
//   measured in the background, and the settings and encoding are stored
//   together as one undoable step (ProjectEdits::SetEncoding()); for the
//   project, the defaults are set as one undoable step, which leaves
//   encodings stale until re-encoded. A change the budget refuses is
//   reported, and the form shows the phrase's settings again.
// - Re-encode encodes the phrase now with the settings shown; Reset puts
//   them back to the project's defaults (or the project's defaults back to
//   the encoder's) and applies that at once; Optimise asks the window to
//   optimise the phrase.
//
// Locked phrases, and phrases with no audio, show their statistics only.
// GUI thread only. The edits, tools, synth and runner must outlive the
// panel; the destructor waits for the runner.
class EncoderPanel : public QWidget {
  Q_OBJECT

 public:
  // Plan §5: the pause after the last change before it is applied.
  static constexpr int kDebounceMs = 600;

  // What the settings form edits.
  enum class Scope { kPhrase, kProjectDefaults };

  EncoderPanel(ProjectEdits& edits, const EncodingTools& tools,
               core::synth::ISynth& synth, core::ITaskRunner& runner,
               QWidget* parent = nullptr);
  ~EncoderPanel() override;
  EncoderPanel(const EncoderPanel&) = delete;
  EncoderPanel& operator=(const EncoderPanel&) = delete;

  // Shows the phrase of `project` with `id`, or nothing for an empty id.
  // Called on every change of the project: a change of setting not yet
  // applied stays in the form. Showing another phrase applies it first.
  void ShowPhrase(const core::project::Project& project,
                  const core::project::PhraseId& id);

  void SetScope(Scope scope);
  Scope CurrentScope() const { return scope_; }

  // Encodes the phrase now with the settings shown (for the phrase's
  // scope; the phrase's own settings for the project's), if it has no
  // frames, they are stale, or a change is waiting; otherwise does nothing.
  void ReEncode();
  // Puts the settings back to their baseline and applies them at once.
  void Reset();
  // Applies a pending change if kDebounceMs have passed since the last
  // one, by the clock; otherwise waits again. The panel's timer calls it.
  void DebounceTick();
  bool HasPendingChange() const { return deadline_us_.has_value(); }
  // Encodings and measurements started and not yet finished.
  int JobsRunning() const { return jobs_running_; }

  EncoderSettingsForm* Form() const { return form_; }
  QualityRatingView* Quality() const { return quality_; }
  QComboBox* ScopeBox() const { return scope_box_; }
  // The preprocessing settings, with their scope chooser: at first inside
  // this panel, above the encoder's settings. It can be put in another
  // widget, which then owns it.
  QWidget* PreprocessPanel() const { return preprocess_panel_; }
  QComboBox* PreprocessScopeBox() const { return preprocess_scope_box_; }
  QLabel* StateLabel() const { return state_; }
  QLabel* FramesLabel() const { return frames_; }
  QLabel* KindsLabel() const { return kinds_; }
  QLabel* LevelLabel() const { return level_; }
  QPushButton* ReEncodeButton() const { return re_encode_; }
  QPushButton* OptimiseButton() const { return optimise_; }
  QPushButton* ResetButton() const { return reset_; }

  // The preprocessing the phrase shown would be encoded with, as the form
  // has it, changes not yet applied included: for showing the recording
  // preprocessed. None for no phrase, a locked one or one without audio.
  core::preprocess::PreprocessOptions ShownPreprocessing() const;

 signals:
  // The Optimise button: optimise the phrase shown.
  void OptimiseRequested(const core::project::PhraseId& id);
  // An encoding or a settings change failed.
  void Failed(const QString& title, const QString& message);
  // An encoding or a measurement finished.
  void JobFinished();
  // The preprocessing of the phrase shown changed (ShownPreprocessing()).
  void PreprocessingChanged(const core::preprocess::PreprocessOptions& options);

 private:
  // What a phrase's frames have been measured to be.
  struct Measured {
    std::vector<core::lpc::Frame> frames;
    EncodingFacts facts;
  };

  void OnEdited();
  void ApplyPending();
  // Encodes the phrase shown with `options`, in the background.
  void StartEncode(const core::encoder::EncoderOptions& options);
  void OnEncoded(int job, const core::project::PhraseId& id,
                 const QString& name, core::project::EncoderOverrides overrides,
                 core::StatusOr<MeasuredEncoding> result);
  void StartMeasure();
  void OnMeasured(const core::project::PhraseId& id,
                  std::vector<core::lpc::Frame> frames, EncodingFacts facts);
  void SetDefaults(const core::encoder::EncoderOptions& options);
  // Shows the settings of the scope in the form.
  void LoadForm();
  void UpdateView();
  // The phrase's settings now: the defaults with its overrides.
  core::encoder::EncoderOptions PhraseOptions() const;
  const EncodingFacts* FactsOfShown() const;
  // The phrase shown has no frames, or frames made from other inputs.
  bool NeedsEncoding() const;

  ProjectEdits* edits_;
  EncodingTools tools_;
  core::synth::ISynth* synth_;
  core::ITaskRunner* runner_;

  QComboBox* scope_box_;
  QWidget* preprocess_panel_;
  QComboBox* preprocess_scope_box_;
  QLabel* state_;
  QLabel* frames_;
  QLabel* kinds_;
  QLabel* level_;
  QualityRatingView* quality_;
  EncoderSettingsForm* form_;
  QPushButton* re_encode_;
  QPushButton* optimise_;
  QPushButton* reset_;
  QTimer* debounce_;

  Scope scope_ = Scope::kPhrase;
  // The project's defaults, and the phrase shown (empty id for none).
  core::encoder::EncoderOptions defaults_;
  core::project::PhraseId id_;
  QString name_;
  std::vector<float> audio_;
  bool locked_ = false;
  core::project::EncoderOverrides overrides_;
  std::optional<core::project::CachedEncoding> encoding_;
  bool current_ = false;

  // A change not yet applied: when it may be, and what and where.
  std::optional<std::int64_t> deadline_us_;
  Scope pending_scope_ = Scope::kPhrase;
  core::encoder::EncoderOptions pending_options_;

  // The preprocessing last given by PreprocessingChanged().
  core::preprocess::PreprocessOptions shown_preprocessing_;

  int jobs_running_ = 0;
  int next_job_ = 0;
  // The latest encoding started for each phrase; earlier ones are
  // dropped when they finish.
  std::map<core::project::PhraseId, int> latest_job_;
  // Encodings running, by phrase.
  std::map<core::project::PhraseId, int> encodes_running_;
  std::map<core::project::PhraseId, Measured> measured_;
  // Measurements running, by phrase.
  std::map<core::project::PhraseId, std::vector<core::lpc::Frame>> measuring_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_ENCODER_PANEL_ENCODER_PANEL_H_
