/*
 * File:        encoder_panel.cpp
 * Module:      encoder_panel
 * Purpose:     The phrase editor's encoder panel: statistics, the quality
 *              scale, the settings form, and re-encoding in the background
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/encoder_panel.h"

#include <QComboBox>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QMetaObject>
#include <QPushButton>
#include <QString>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_statistics.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/encoder_panel/encoder_settings_form.h"
#include "gui/encoder_panel/encoding_summary.h"
#include "gui/encoder_panel/phrase_encoder.h"
#include "gui/encoder_panel/quality_scale_view.h"
#include "gui/undo/project_edits.h"

namespace speak5220::app {

namespace project = core::project;
using core::encoder::EncoderOptions;

namespace {

constexpr std::int64_t kMicrosecondsPerMillisecond = 1000;
// The scope chooser's entries, in Scope order.
constexpr int kPhraseScopeIndex = 0;
constexpr int kDefaultsScopeIndex = 1;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

}  // namespace

EncoderPanel::EncoderPanel(ProjectEdits& edits, const EncodingTools& tools,
                           core::synth::ISynth& synth,
                           core::ITaskRunner& runner, QWidget* parent)
    : QWidget(parent),
      edits_(&edits),
      tools_(tools),
      synth_(&synth),
      runner_(&runner),
      scope_box_(new QComboBox(this)),
      state_(new QLabel(this)),
      frames_(new QLabel(this)),
      kinds_(new QLabel(this)),
      level_(new QLabel(this)),
      quality_(new QualityScaleView(this)),
      form_(new EncoderSettingsForm(this)),
      re_encode_(new QPushButton(QStringLiteral("Re-&encode"), this)),
      optimise_(new QPushButton(QStringLiteral("&Optimise..."), this)),
      reset_(
          new QPushButton(QStringLiteral("Reset to Project Defaults"), this)),
      debounce_(new QTimer(this)) {
  scope_box_->addItem(QStringLiteral("This phrase"));
  scope_box_->addItem(QStringLiteral("Project defaults"));
  scope_box_->setToolTip(QStringLiteral(
      "Edit this phrase's settings, or the defaults every phrase starts "
      "from"));
  QFont bold = state_->font();
  bold.setBold(true);
  state_->setFont(bold);
  for (QLabel* label : {frames_, kinds_, level_}) {
    label->setTextInteractionFlags(Qt::TextSelectableByMouse);
  }
  re_encode_->setToolTip(
      QStringLiteral("Encode the phrase now with the settings shown"));
  optimise_->setToolTip(QStringLiteral(
      "Search the settings for the best quality within the budget"));
  debounce_->setSingleShot(true);

  auto* scope_row = new QHBoxLayout;
  auto* scope_label = new QLabel(QStringLiteral("&Settings for"), this);
  scope_label->setBuddy(scope_box_);
  scope_row->addWidget(scope_label);
  scope_row->addWidget(scope_box_);
  scope_row->addStretch();
  auto* buttons = new QHBoxLayout;
  buttons->addWidget(re_encode_);
  buttons->addWidget(optimise_);
  buttons->addStretch();
  buttons->addWidget(reset_);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(state_);
  layout->addWidget(frames_);
  layout->addWidget(kinds_);
  layout->addWidget(level_);
  layout->addWidget(quality_);
  layout->addLayout(scope_row);
  layout->addWidget(form_);
  layout->addLayout(buttons);
  layout->addStretch();

  connect(form_, &EncoderSettingsForm::Edited, this, &EncoderPanel::OnEdited);
  connect(debounce_, &QTimer::timeout, this, &EncoderPanel::DebounceTick);
  connect(scope_box_, &QComboBox::currentIndexChanged, this, [this](int index) {
    SetScope(index == kDefaultsScopeIndex ? Scope::kProjectDefaults
                                          : Scope::kPhrase);
  });
  connect(re_encode_, &QPushButton::clicked, this, &EncoderPanel::ReEncode);
  connect(reset_, &QPushButton::clicked, this, &EncoderPanel::Reset);
  connect(optimise_, &QPushButton::clicked, this,
          [this] { emit OptimiseRequested(id_); });
  LoadForm();
  UpdateView();
}

EncoderPanel::~EncoderPanel() {
  // Jobs post back to this panel.
  runner_->Wait();
}

void EncoderPanel::ShowPhrase(const project::Project& project,
                              const project::PhraseId& id) {
  const project::Phrase* phrase = id.empty() ? nullptr : project.FindPhrase(id);
  const project::PhraseId shown = phrase != nullptr ? phrase->id : "";
  if (shown != id_ && pending_scope_ == Scope::kPhrase) {
    // The change belongs to the phrase it was made on.
    ApplyPending();
  }
  defaults_ = project.Settings().default_encoder;
  if (phrase == nullptr) {
    id_.clear();
    name_.clear();
    audio_.clear();
    locked_ = false;
    overrides_.clear();
    encoding_.reset();
    current_ = false;
  } else {
    if (phrase->id != id_) {
      id_ = phrase->id;
      // A phrase's audio never changes once imported (plan §3.1).
      audio_ = phrase->audio;
    }
    name_ = QString::fromStdString(phrase->name);
    locked_ = phrase->locked;
    overrides_ = phrase->encoder;
    encoding_ = phrase->encoding;
    const core::StatusOr<bool> current =
        project::EncodingCache::IsCurrent(*phrase, defaults_);
    current_ = current.IsOk() && *current;
  }
  // A change waiting to be applied, or being encoded, stays in the form.
  const bool encoding_shown =
      scope_ == Scope::kPhrase && encodes_running_.contains(id_);
  if (!HasPendingChange() && !encoding_shown) {
    LoadForm();
  }
  UpdateView();
  StartMeasure();
}

void EncoderPanel::SetScope(Scope scope) {
  if (scope == scope_) {
    return;
  }
  ApplyPending();
  scope_ = scope;
  scope_box_->setCurrentIndex(scope == Scope::kProjectDefaults
                                  ? kDefaultsScopeIndex
                                  : kPhraseScopeIndex);
  reset_->setText(scope == Scope::kProjectDefaults
                      ? QStringLiteral("Reset to Encoder Defaults")
                      : QStringLiteral("Reset to Project Defaults"));
  LoadForm();
  UpdateView();
}

void EncoderPanel::ReEncode() {
  if (HasPendingChange() && pending_scope_ == Scope::kPhrase) {
    // The change pending is what would be encoded.
    ApplyPending();
    return;
  }
  const bool pending = HasPendingChange();
  ApplyPending();
  // Only a phrase whose frames are missing or out of date needs encoding;
  // a change of defaults just applied may have made it so.
  if (!pending && !NeedsEncoding()) {
    return;
  }
  StartEncode(scope_ == Scope::kPhrase ? form_->Options() : PhraseOptions());
}

bool EncoderPanel::NeedsEncoding() const {
  return !encoding_.has_value() || !current_;
}

void EncoderPanel::Reset() {
  deadline_us_.reset();
  debounce_->stop();
  if (scope_ == Scope::kProjectDefaults) {
    SetDefaults(EncoderOptions{});
    return;
  }
  form_->SetOptions(defaults_, defaults_,
                    QStringLiteral("the project default"));
  StartEncode(defaults_);
}

void EncoderPanel::DebounceTick() {
  if (!deadline_us_.has_value()) {
    return;
  }
  const std::int64_t now = tools_.clock->NowMicroseconds();
  if (now < *deadline_us_) {
    const std::int64_t wait = *deadline_us_ - now;
    debounce_->start(static_cast<int>((wait + kMicrosecondsPerMillisecond - 1) /
                                      kMicrosecondsPerMillisecond));
    return;
  }
  ApplyPending();
}

void EncoderPanel::OnEdited() {
  pending_scope_ = scope_;
  pending_options_ = form_->Options();
  deadline_us_ = tools_.clock->NowMicroseconds() +
                 (kDebounceMs * kMicrosecondsPerMillisecond);
  debounce_->start(kDebounceMs);
  UpdateView();
}

void EncoderPanel::ApplyPending() {
  if (!deadline_us_.has_value()) {
    return;
  }
  deadline_us_.reset();
  debounce_->stop();
  if (pending_scope_ == Scope::kProjectDefaults) {
    SetDefaults(pending_options_);
  } else {
    StartEncode(pending_options_);
  }
}

void EncoderPanel::StartEncode(const EncoderOptions& options) {
  if (id_.empty() || audio_.empty() || locked_) {
    return;
  }
  const int job = ++next_job_;
  latest_job_[id_] = job;
  ++jobs_running_;
  ++encodes_running_[id_];
  const AnchorKey anchor_key = AnchorKeyOf(id_, options);
  const std::optional<double> anchor = KnownAnchor(anchor_key);
  // Plan §5: encoded and measured in the background; the result comes back
  // to this thread queued (AGENTS.md §4.3.3).
  runner_->Post([this, job, anchor_key, name = name_, audio = audio_, options,
                 overrides = project::OverridesFor(defaults_, options), anchor,
                 tools = tools_, synth = synth_]() mutable {
    auto result = std::make_shared<core::StatusOr<MeasuredEncoding>>(
        EncodePhrase(tools, *synth, audio, options, anchor));
    QMetaObject::invokeMethod(
        this,
        [this, job, anchor_key, name, overrides = std::move(overrides),
         result]() mutable {
          OnEncoded(job, anchor_key, name, std::move(overrides),
                    std::move(*result));
        },
        Qt::QueuedConnection);
  });
  UpdateView();
}

EncoderPanel::AnchorKey EncoderPanel::AnchorKeyOf(
    const project::PhraseId& id, const EncoderOptions& options) {
  return {id, core::encoder::EncoderOptionText(options, "squelch_db")};
}

std::optional<double> EncoderPanel::KnownAnchor(const AnchorKey& key) const {
  if (const auto found = anchors_.find(key); found != anchors_.end()) {
    return found->second;
  }
  return std::nullopt;
}

void EncoderPanel::OnEncoded(int job, const AnchorKey& anchor_key,
                             const QString& name,
                             project::EncoderOverrides overrides,
                             core::StatusOr<MeasuredEncoding> result) {
  const project::PhraseId& id = anchor_key.first;
  --jobs_running_;
  if (--encodes_running_[id] == 0) {
    encodes_running_.erase(id);
  }
  const auto latest = latest_job_.find(id);
  if (latest == latest_job_.end() || latest->second != job) {
    // A later encoding of the phrase replaces this one.
    emit JobFinished();
    return;
  }
  latest_job_.erase(latest);
  if (!result.IsOk()) {
    emit Failed(QStringLiteral("Encode %1").arg(name),
                ToQString(result.GetStatus().Message()));
    if (id == id_) {
      LoadForm();
    }
    UpdateView();
    emit JobFinished();
    return;
  }
  const EncodingFacts& facts = result->facts;
  measured_[id] = {.frames = result->encoding.frames, .facts = facts};
  if (facts.anchor.has_value()) {
    anchors_[anchor_key] = *facts.anchor;
  }
  const bool unchanged = id == id_ && overrides == overrides_ &&
                         encoding_.has_value() &&
                         *encoding_ == result->encoding;
  if (!unchanged) {
    if (const core::Status status =
            edits_->SetEncoding(id, {.overrides = std::move(overrides),
                                     .encoding = std::move(result->encoding)});
        !status.IsOk()) {
      // A phrase gone (another project opened meanwhile) needs no message.
      if (status.Code() != core::StatusCode::kNotFound) {
        emit Failed(QStringLiteral("Change Encoder Settings"),
                    ToQString(status.Message()));
      }
      if (id == id_ && !HasPendingChange()) {
        LoadForm();
      }
    }
  }
  UpdateView();
  emit JobFinished();
}

void EncoderPanel::StartMeasure() {
  if (id_.empty() || audio_.empty() || !encoding_.has_value() ||
      encoding_->frames.empty() || FactsOfShown() != nullptr) {
    return;
  }
  if (const auto running = measuring_.find(id_);
      running != measuring_.end() && running->second == encoding_->frames) {
    return;
  }
  // Measured against the source as the phrase's settings squelch it.
  const core::StatusOr<EncoderOptions> options =
      project::EffectiveEncoderOptions(defaults_, overrides_);
  if (!options.IsOk()) {
    return;
  }
  measuring_[id_] = encoding_->frames;
  ++jobs_running_;
  const AnchorKey anchor_key = AnchorKeyOf(id_, *options);
  // The anchor stored with the frames is of their squelch, which is the
  // settings' only while they are current.
  std::optional<double> anchor = current_ ? encoding_->anchor : std::nullopt;
  if (const std::optional<double> known = KnownAnchor(anchor_key)) {
    anchor = known;
  }
  runner_->Post([this, anchor_key, audio = audio_, frames = encoding_->frames,
                 options = *options, anchor, tools = tools_, synth = synth_] {
    auto facts = std::make_shared<EncodingFacts>(
        MeasureFrames(tools, *synth, audio, options, frames, anchor));
    QMetaObject::invokeMethod(
        this,
        [this, anchor_key, frames, facts] {
          OnMeasured(anchor_key, frames, *facts);
        },
        Qt::QueuedConnection);
  });
  UpdateView();
}

void EncoderPanel::OnMeasured(const AnchorKey& anchor_key,
                              std::vector<core::lpc::Frame> frames,
                              EncodingFacts facts) {
  const project::PhraseId& id = anchor_key.first;
  --jobs_running_;
  if (const auto running = measuring_.find(id);
      running != measuring_.end() && running->second == frames) {
    measuring_.erase(running);
  }
  if (facts.anchor.has_value()) {
    anchors_[anchor_key] = *facts.anchor;
  }
  measured_[id] = {.frames = std::move(frames), .facts = facts};
  UpdateView();
  emit JobFinished();
}

void EncoderPanel::SetDefaults(const EncoderOptions& options) {
  if (options == defaults_) {
    LoadForm();
    UpdateView();
    return;
  }
  if (const core::Status status = edits_->SetDefaultEncoder(options);
      !status.IsOk()) {
    emit Failed(QStringLiteral("Change Project Defaults"),
                ToQString(status.Message()));
    LoadForm();
    UpdateView();
  }
}

void EncoderPanel::LoadForm() {
  if (scope_ == Scope::kProjectDefaults) {
    form_->SetOptions(defaults_, EncoderOptions{},
                      QStringLiteral("the encoder's default"));
  } else {
    form_->SetOptions(PhraseOptions(), defaults_,
                      QStringLiteral("the project default"));
  }
}

std::optional<double> EncoderPanel::ShownSquelch() const {
  if (id_.empty() || locked_ || audio_.empty()) {
    return std::nullopt;
  }
  if (scope_ == Scope::kPhrase) {
    return form_->Options().squelch_db;
  }
  // The form holds the project's defaults; the phrase may set its own.
  const core::StatusOr<EncoderOptions> options =
      project::EffectiveEncoderOptions(form_->Options(), overrides_);
  return options.IsOk() ? options->squelch_db : std::nullopt;
}

void EncoderPanel::UpdateView() {
  if (const std::optional<double> squelch = ShownSquelch();
      squelch != shown_squelch_) {
    shown_squelch_ = squelch;
    emit SquelchChanged(squelch);
  }
  const bool has_phrase = !id_.empty();
  const bool editable = has_phrase && !locked_ && !audio_.empty();
  const bool encoding_shown = encodes_running_.contains(id_);
  form_->setEnabled(scope_ == Scope::kProjectDefaults || editable);
  // Re-encoding is offered only when there is something to redo: no
  // frames, frames out of date, or a change waiting to be applied.
  re_encode_->setEnabled(editable && !encoding_shown &&
                         (NeedsEncoding() || HasPendingChange()));
  re_encode_->setText(encoding_.has_value() ? QStringLiteral("Re-&encode")
                                            : QStringLiteral("&Encode"));
  optimise_->setEnabled(editable);
  const EncoderOptions baseline =
      scope_ == Scope::kProjectDefaults ? EncoderOptions{} : defaults_;
  reset_->setEnabled((scope_ == Scope::kProjectDefaults || editable) &&
                     form_->Options() != baseline);

  QString state;
  if (!has_phrase) {
    state = QStringLiteral("Select one phrase to see its encoding");
  } else if (locked_) {
    state = QStringLiteral("Locked: imported frames are never re-encoded");
  } else if (audio_.empty()) {
    state = QStringLiteral("No source audio to encode");
  } else if (HasPendingChange()) {
    state = QStringLiteral("Settings changed: applied after a pause...");
  } else if (encoding_shown) {
    state = QStringLiteral("Encoding...");
  } else if (!encoding_.has_value()) {
    state = QStringLiteral("Not encoded yet");
  } else if (!current_) {
    state =
        QStringLiteral("Stale: made with other settings; re-encode to update");
  } else {
    state = QStringLiteral("Up to date");
  }
  state_->setText(state);

  const EncodingFacts* facts = FactsOfShown();
  if (encoding_.has_value() && !encoding_->frames.empty()) {
    const core::lpc::FrameStatistics stats =
        core::lpc::SummariseFrames(encoding_->frames);
    frames_->setText(FramesText(stats));
    kinds_->setText(KindsText(stats));
    level_->setText(facts != nullptr
                        ? LevelText(facts->peak_code, facts->levels)
                        : QString());
    std::optional<double> pesq = encoding_->pesq;
    std::optional<double> anchor = encoding_->anchor;
    if (facts != nullptr) {
      pesq = pesq.has_value() ? pesq : facts->pesq;
      anchor = anchor.has_value() ? anchor : facts->anchor;
    }
    if (pesq.has_value()) {
      quality_->SetScores(pesq, anchor);
    } else if (measuring_.contains(id_)) {
      quality_->SetPlaceholder(QStringLiteral("Measuring quality..."));
    } else {
      quality_->SetScores(std::nullopt, std::nullopt);
    }
  } else {
    frames_->setText(has_phrase ? QStringLiteral("No frames") : QString());
    kinds_->clear();
    level_->clear();
    quality_->SetPlaceholder(has_phrase ? QStringLiteral("Not encoded")
                                        : QString());
  }
}

EncoderOptions EncoderPanel::PhraseOptions() const {
  core::StatusOr<EncoderOptions> options =
      project::EffectiveEncoderOptions(defaults_, overrides_);
  return options.IsOk() ? *options : defaults_;
}

const EncodingFacts* EncoderPanel::FactsOfShown() const {
  if (!encoding_.has_value()) {
    return nullptr;
  }
  const auto found = measured_.find(id_);
  if (found == measured_.end() || found->second.frames != encoding_->frames) {
    return nullptr;
  }
  return &found->second.facts;
}

}  // namespace speak5220::app
