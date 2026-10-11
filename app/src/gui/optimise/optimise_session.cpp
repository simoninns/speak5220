/*
 * File:        optimise_session.cpp
 * Module:      optimise
 * Purpose:     Runs Optimise over one or more phrases in the background,
 *              reports its progress, and applies or discards the result
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/optimise/optimise_session.h"

#include <QMetaObject>
#include <QObject>
#include <QString>
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/optimiser/optimiser.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/undo/project_commands.h"
#include "gui/undo/project_edits.h"

namespace speak5220::app {

namespace project = core::project;
namespace optimiser = core::optimiser;

namespace {

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

OptimiseSession::OptimiseSession(ProjectEdits& edits,
                                 const EncodingTools& tools,
                                 core::ITaskRunner& workers, QObject* parent)
    : QObject(parent), edits_(&edits), tools_(tools), workers_(&workers) {}

OptimiseSession::~OptimiseSession() {
  Cancel();
  // The search posts back to this session.
  tools_.optimiser_driver->Wait();
}

core::Status OptimiseSession::Start(const project::Project& project,
                                    const std::vector<project::PhraseId>& ids,
                                    const optimiser::OptimiseLimits& limits,
                                    const optimiser::SearchPlan& plan) {
  if (running_) {
    return core::FailedPreconditionError("Optimise is already running");
  }
  std::vector<int> indices;
  for (const project::PhraseId& id : ids) {
    const std::optional<int> index = project.IndexOf(id);
    if (!index.has_value()) {
      continue;
    }
    const project::Phrase& phrase =
        project.Phrases()[static_cast<std::size_t>(*index)];
    if (!phrase.locked && !phrase.audio.empty()) {
      indices.push_back(*index);
    }
  }
  std::ranges::sort(indices);
  const auto [first, last] = std::ranges::unique(indices);
  indices.erase(first, last);
  if (indices.empty()) {
    return core::FailedPreconditionError(
        "none of the phrases can be optimised: they are locked or have no "
        "audio");
  }
  phrases_.clear();
  history_.clear();
  std::vector<project::PhraseId> order;
  for (const int index : indices) {
    const project::Phrase& phrase =
        project.Phrases()[static_cast<std::size_t>(index)];
    PhraseOptimisation entry;
    entry.id = phrase.id;
    entry.name = QString::fromStdString(phrase.name);
    phrases_.push_back(std::move(entry));
    order.push_back(phrase.id);
  }
  current_ = 0;
  evaluated_ = 0;
  expected_ = 0;
  cancelled_ = false;
  running_ = true;
  cancel_ = std::make_shared<std::atomic<bool>>(false);
  core::Logger(core::kLogModuleGui)
      ->info("optimising {} phrase(s)", order.size());

  // Plan §3.3: a cancellable background job. The project is copied, so
  // the search sees it as it was, with its own results added as it goes.
  tools_.optimiser_driver->Post(
      [this, working = project, order = std::move(order), limits, plan,
       tools = tools_, workers = workers_, cancel = cancel_]() mutable {
        optimiser::Optimiser search(*tools.encoder, *tools.quality, *workers);
        for (std::size_t p = 0; p < order.size() && !cancel->load(); ++p) {
          const int phrase_index = static_cast<int>(p);
          const project::Phrase& phrase = *working.FindPhrase(order[p]);
          PhraseOptimisation result;
          result.id = phrase.id;
          result.name = QString::fromStdString(phrase.name);
          const std::vector<double> source(phrase.audio.begin(),
                                           phrase.audio.end());
          const core::StatusOr<core::encoder::EncoderOptions> options =
              project::EffectiveEncoderOptions(
                  working.Settings().default_encoder, phrase.encoder);
          const core::StatusOr<project::PhraseBudgetCheck> check =
              working.BudgetCheckFor(phrase.id);
          if (!options.IsOk() || !check.IsOk()) {
            result.problem =
                ToQString(options.IsOk() ? check.GetStatus().Message()
                                         : options.GetStatus().Message());
            QMetaObject::invokeMethod(
                this,
                [this, phrase_index, result] {
                  OnPhraseFinished(phrase_index, result);
                },
                Qt::QueuedConnection);
            continue;
          }
          // The anchor of the source as the encodings are scored against
          // it, squelched by the phrase's squelch, which Optimise keeps.
          if (core::StatusOr<double> anchor = tools.anchor->Score(
                  core::encoder::SquelchedSource(source, *options));
              anchor.IsOk()) {
            result.anchor = *anchor;
          }
          std::size_t sent = 0;
          auto report = [&](const optimiser::OptimiseResult& so_far,
                            int evaluated, int expected) {
            std::vector<optimiser::Candidate> fresh(
                so_far.history.begin() + static_cast<std::ptrdiff_t>(sent),
                so_far.history.end());
            sent = so_far.history.size();
            std::optional<optimiser::Candidate> start;
            std::optional<optimiser::Candidate> best;
            if (so_far.Start() != nullptr) {
              start = *so_far.Start();
            }
            if (so_far.Best() != nullptr) {
              best = *so_far.Best();
            }
            QMetaObject::invokeMethod(
                this,
                [this, phrase_index, anchor = result.anchor, evaluated,
                 expected, start = std::move(start), best = std::move(best),
                 fresh = std::move(fresh)]() mutable {
                  OnProgress(phrase_index, anchor, evaluated, expected,
                             std::move(start), std::move(best),
                             std::move(fresh));
                },
                Qt::QueuedConnection);
          };
          auto check_bytes = [check = *check](int bytes) {
            return check.Check(bytes);
          };
          core::StatusOr<optimiser::OptimiseResult> run =
              search.Run({.source = source,
                          .start = *options,
                          .limits = limits,
                          .budget = check_bytes,
                          .plan = plan},
                         cancel.get(), report);
          if (!run.IsOk()) {
            result.problem = ToQString(run.GetStatus().Message());
          } else {
            // The last batch, which the progress callback had not seen.
            report(*run, static_cast<int>(run->history.size()),
                   static_cast<int>(run->history.size()));
            if (run->Start() != nullptr) {
              result.start = *run->Start();
            }
            if (run->Best() != nullptr) {
              result.best = *run->Best();
            }
            if (!run->cancelled && result.best.has_value() &&
                result.best->options != *options) {
              project::CachedEncoding encoding =
                  project::EncodingCache::MakeEncoding(
                      phrase.audio, result.best->options, run->best_frames);
              encoding.pesq = result.best->score;
              encoding.anchor = result.anchor;
              project::EncodingChange change{
                  .overrides = project::OverridesFor(
                      working.Settings().default_encoder, result.best->options),
                  .encoding = std::move(encoding)};
              // Kept in the copy, so later phrases are checked against it.
              if (const core::Status status =
                      working.SetEncoding(phrase.id, change).GetStatus();
                  status.IsOk()) {
                result.change = std::move(change);
              } else {
                result.problem = ToQString(status.Message());
              }
            }
          }
          QMetaObject::invokeMethod(
              this,
              [this, phrase_index, result = std::move(result)]() mutable {
                OnPhraseFinished(phrase_index, std::move(result));
              },
              Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(
            this, [this, cancelled = cancel->load()] { OnFinished(cancelled); },
            Qt::QueuedConnection);
      });
  return core::OkStatus();
}

void OptimiseSession::Cancel() {
  if (cancel_ != nullptr) {
    cancel_->store(true);
  }
}

int OptimiseSession::ChangeCount() const {
  return static_cast<int>(std::ranges::count_if(
      phrases_,
      [](const PhraseOptimisation& p) { return p.change.has_value(); }));
}

core::Status OptimiseSession::Accept() {
  if (running_) {
    return core::FailedPreconditionError("Optimise is still running");
  }
  if (cancelled_) {
    return core::FailedPreconditionError("Optimise was cancelled");
  }
  SetEncodingsCommand::Changes changes;
  for (const PhraseOptimisation& phrase : phrases_) {
    if (phrase.change.has_value()) {
      changes.emplace_back(phrase.id, *phrase.change);
    }
  }
  if (changes.empty()) {
    return core::FailedPreconditionError("Optimise found nothing to change");
  }
  const QString text =
      changes.size() == 1
          ? QStringLiteral("Optimise %1")
                .arg(std::ranges::find_if(phrases_,
                                          [](const PhraseOptimisation& p) {
                                            return p.change.has_value();
                                          })
                         ->name)
          : QStringLiteral("Optimise %1 Phrases").arg(changes.size());
  if (core::Status status = edits_->SetEncodings(std::move(changes), text);
      !status.IsOk()) {
    return status;
  }
  Discard();
  return core::OkStatus();
}

void OptimiseSession::Discard() {
  for (PhraseOptimisation& phrase : phrases_) {
    phrase.change.reset();
  }
}

void OptimiseSession::OnProgress(int phrase, std::optional<double> anchor,
                                 int evaluated, int expected,
                                 std::optional<optimiser::Candidate> start,
                                 std::optional<optimiser::Candidate> best,
                                 std::vector<optimiser::Candidate> fresh) {
  if (phrase < 0 || std::cmp_greater_equal(phrase, phrases_.size())) {
    return;
  }
  current_ = phrase;
  evaluated_ = evaluated;
  expected_ = expected;
  PhraseOptimisation& entry = phrases_[static_cast<std::size_t>(phrase)];
  entry.anchor = anchor;
  entry.start = std::move(start);
  entry.best = std::move(best);
  if (!fresh.empty()) {
    const int first = static_cast<int>(history_.size());
    for (optimiser::Candidate& candidate : fresh) {
      history_.push_back({.phrase = phrase, .candidate = std::move(candidate)});
    }
    emit HistoryAdded(first);
  }
  emit ProgressChanged();
}

void OptimiseSession::OnPhraseFinished(int phrase, PhraseOptimisation result) {
  if (phrase < 0 || std::cmp_greater_equal(phrase, phrases_.size())) {
    return;
  }
  phrases_[static_cast<std::size_t>(phrase)] = std::move(result);
  emit PhraseFinished(phrase);
}

void OptimiseSession::OnFinished(bool cancelled) {
  running_ = false;
  cancelled_ = cancelled;
  if (cancelled) {
    // Plan Phase 7: cancelling leaves the settings as they were.
    Discard();
  }
  core::Logger(core::kLogModuleGui)
      ->info("Optimise {}: {} phrase(s) to change",
             cancelled ? "cancelled" : "finished", ChangeCount());
  emit Finished();
}

}  // namespace speak5220::app
