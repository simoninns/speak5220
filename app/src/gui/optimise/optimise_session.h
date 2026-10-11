/*
 * File:        optimise_session.h
 * Module:      optimise
 * Purpose:     Runs Optimise over one or more phrases in the background,
 *              reports its progress, and applies or discards the result
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_OPTIMISE_OPTIMISE_SESSION_H_
#define SPEAK5220_GUI_OPTIMISE_OPTIMISE_SESSION_H_

#include <QObject>
#include <QString>
#include <atomic>
#include <memory>
#include <optional>
#include <vector>

#include "core/optimiser/optimiser.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_editor.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/encoder_panel/phrase_encoder.h"
#include "gui/undo/project_edits.h"

namespace speak5220::app {

// What Optimise did with one phrase.
struct PhraseOptimisation {
  core::project::PhraseId id;
  QString name;
  // The phrase's settings when Optimise started, measured, and the best
  // candidate found (none if none was within the limits).
  std::optional<core::optimiser::Candidate> start;
  std::optional<core::optimiser::Candidate> best;
  std::optional<double> anchor;
  // The settings and encoding to store, when the best differs from the
  // start.
  std::optional<core::project::EncodingChange> change;
  // Why the phrase could not be optimised, or its change not kept.
  QString problem;
};

// One row of the candidate history: a phrase (an index into Phrases())
// and one of its candidates.
struct OptimiseHistoryRow {
  int phrase = 0;
  core::optimiser::Candidate candidate;
};

// Plan §3.3 and Phase 7: Optimise, and Optimise all over a selection, in
// the GUI. Start() copies the project and runs core::optimiser::Optimiser
// on each phrase in list order, one at a time, as a job on
// EncodingTools::optimiser_driver; each search's candidates run on the
// window's ITaskRunner. Each phrase's budget is checked against the
// project with the earlier phrases' results in it, so their results fit
// together. Progress and the history come back to the GUI thread queued.
//
// Nothing changes until Accept(), which stores every phrase's new settings
// and encoding (with its PESQ and anchor) as one undoable step;
// Discard(), or cancelling, leaves the project as it was.
//
// GUI thread only. The edits, tools and runner must outlive the session;
// the destructor cancels a search and waits for it.
class OptimiseSession : public QObject {
  Q_OBJECT

 public:
  OptimiseSession(ProjectEdits& edits, const EncodingTools& tools,
                  core::ITaskRunner& workers, QObject* parent = nullptr);
  ~OptimiseSession() override;
  OptimiseSession(const OptimiseSession&) = delete;
  OptimiseSession& operator=(const OptimiseSession&) = delete;

  // Starts optimising the phrases of `project` with `ids` that can be
  // (unlocked, with audio), in list order. Fails if a search is running
  // or no phrase can be optimised.
  core::Status Start(const core::project::Project& project,
                     const std::vector<core::project::PhraseId>& ids,
                     const core::optimiser::OptimiseLimits& limits,
                     const core::optimiser::SearchPlan& plan = {});
  // Stops the search after the candidates running; nothing is kept.
  void Cancel();
  // Stores the results as one step. Fails if the search has not finished,
  // was cancelled, found nothing to change, or the project refuses.
  core::Status Accept();
  // Forgets the results.
  void Discard();

  bool IsRunning() const { return running_; }
  bool WasCancelled() const { return cancelled_; }
  // Phrases whose results would change them.
  int ChangeCount() const;

  const std::vector<PhraseOptimisation>& Phrases() const { return phrases_; }
  const std::vector<OptimiseHistoryRow>& History() const { return history_; }
  // The phrase being optimised (an index into Phrases()), and its
  // candidates evaluated and the most there will be.
  int CurrentPhrase() const { return current_; }
  int Evaluated() const { return evaluated_; }
  int Expected() const { return expected_; }

 signals:
  // Evaluated(), Expected() or a phrase's start or best changed.
  void ProgressChanged();
  // Rows were added to History(), from `first`.
  void HistoryAdded(int first);
  // A phrase's search finished.
  void PhraseFinished(int phrase);
  // The whole search finished or was cancelled.
  void Finished();

 private:
  void OnProgress(int phrase, std::optional<double> anchor, int evaluated,
                  int expected, std::optional<core::optimiser::Candidate> start,
                  std::optional<core::optimiser::Candidate> best,
                  std::vector<core::optimiser::Candidate> fresh);
  void OnPhraseFinished(int phrase, PhraseOptimisation result);
  void OnFinished(bool cancelled);

  ProjectEdits* edits_;
  EncodingTools tools_;
  core::ITaskRunner* workers_;
  // Shared with the running search, which outlives neither.
  std::shared_ptr<std::atomic<bool>> cancel_;
  bool running_ = false;
  bool cancelled_ = false;
  std::vector<PhraseOptimisation> phrases_;
  std::vector<OptimiseHistoryRow> history_;
  int current_ = 0;
  int evaluated_ = 0;
  int expected_ = 0;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_OPTIMISE_OPTIMISE_SESSION_H_
