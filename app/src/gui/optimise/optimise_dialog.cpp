/*
 * File:        optimise_dialog.cpp
 * Module:      optimise
 * Purpose:     The Optimise window: limits, progress, the best so far
 *              against the start, the candidate history, and accept or
 *              revert
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/optimise/optimise_dialog.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDoubleSpinBox>
#include <QEvent>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMetaObject>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QString>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_statistics.h"
#include "core/optimiser/optimiser.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/encoder_panel/encoding_summary.h"
#include "gui/encoder_panel/quality_rating_view.h"
#include "gui/optimise/optimise_session.h"
#include "gui/playback/playback_icon.h"
#include "gui/playback/player.h"

namespace speak5220::app {

namespace project = core::project;
namespace optimiser = core::optimiser;
namespace quality = core::quality;
namespace synth = core::synth;

namespace {

// The progress bar counts each phrase in this many steps.
constexpr int kStepsPerPhrase = 1000;
// The largest byte limit offered: a whole PHROM's words.
constexpr int kMaxLimit = 16128;
// The bit-rate weight's range and default, in overall score per kbit/s:
// at the default, 1 kbit/s less is worth 0.03 of the score.
constexpr double kMaxWeight = 3.0;
constexpr double kDefaultWeight = 0.03;
constexpr double kWeightStep = 0.01;
constexpr int kWeightDecimals = 3;
// Space above and below a history row's text, in pixels.
constexpr int kRowPadding = 6;
// The window's initial size.
constexpr int kInitialWidth = 640;
constexpr int kInitialHeight = 480;

// The history's columns.
constexpr int kPhraseColumn = 0;
constexpr int kChangesColumn = 3;
constexpr int kOutcomeColumn = 7;
constexpr int kColumnCount = 8;

// Full scale of a 16-bit sample.
constexpr double kFullScale = 32768.0;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// The encoded audio from `pin` at the recording's level, as the encoded
// waveform pane plays it.
std::vector<std::int16_t> AtRecordingLevel(const quality::EncodedAudio& audio,
                                           synth::OutputPin pin) {
  std::vector<float> matched;
  matched.reserve(audio.samples.size());
  for (const std::int16_t v : synth::PinSamples(pin, audio.samples)) {
    matched.push_back(static_cast<float>(v * audio.level_gain / kFullScale));
  }
  return ToPcm16(matched);
}

QToolButton* MakePlayButton(const QString& label, QWidget* parent) {
  auto* button = new QToolButton(parent);
  button->setText(label);
  button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  return button;
}

}  // namespace

OptimiseDialog::OptimiseDialog(OptimiseSession& session, Player& player,
                               core::synth::ISynth& synth,
                               core::ITaskRunner& runner, QWidget* parent)
    : QDialog(parent),
      session_(&session),
      limit_box_(new QCheckBox(QStringLiteral("&Limit each phrase to"), this)),
      limit_(new QSpinBox(this)),
      weight_box_(
          new QCheckBox(QStringLiteral("&Trade quality for size:"), this)),
      weight_(new QDoubleSpinBox(this)),
      status_(new QLabel(this)),
      progress_(new QProgressBar(this)),
      quality_(new QualityRatingView(this)),
      best_(new QLabel(this)),
      show_history_(
          new QCheckBox(QStringLiteral("Show every &candidate"), this)),
      history_(new QTableWidget(0, kColumnCount, this)),
      start_(new QPushButton(QStringLiteral("&Start"), this)),
      cancel_(new QPushButton(QStringLiteral("Cancel"), this)),
      accept_(new QPushButton(QStringLiteral("&Accept"), this)),
      revert_(new QPushButton(QStringLiteral("&Revert"), this)),
      close_(new QPushButton(QStringLiteral("Close"), this)),
      listen_row_(new QWidget(this)),
      listen_phrase_(new QComboBox(listen_row_)),
      play_original_(MakePlayButton(QStringLiteral("&Original"), listen_row_)),
      play_optimised_(
          MakePlayButton(QStringLiteral("O&ptimised"), listen_row_)),
      listen_note_(new QLabel(listen_row_)),
      player_(&player),
      synth_(&synth),
      runner_(&runner) {
  setWindowModality(Qt::WindowModal);
  resize(kInitialWidth, kInitialHeight);
  limit_->setRange(1, kMaxLimit);
  limit_->setSuffix(QStringLiteral(" bytes"));
  limit_->setEnabled(false);
  limit_->setAccessibleName(QStringLiteral("Byte limit per phrase"));
  limit_box_->setToolTip(QStringLiteral(
      "Only settings whose frames fit in this many bytes are chosen"));
  weight_->setRange(0.0, kMaxWeight);
  weight_->setDecimals(kWeightDecimals);
  weight_->setSingleStep(kWeightStep);
  weight_->setValue(kDefaultWeight);
  weight_->setSuffix(QStringLiteral(" score per kbit/s"));
  weight_->setEnabled(false);
  weight_->setAccessibleName(QStringLiteral("Size weight"));
  weight_box_->setToolTip(QStringLiteral(
      "Choose the settings with the best quality score less this much for "
      "each kbit/s, so a smaller encoding can win"));
  connect(limit_box_, &QCheckBox::toggled, limit_, &QSpinBox::setEnabled);
  connect(weight_box_, &QCheckBox::toggled, weight_,
          &QDoubleSpinBox::setEnabled);
  best_->setWordWrap(true);
  status_->setWordWrap(true);
  history_->setHorizontalHeaderLabels(
      {QStringLiteral("Phrase"), QStringLiteral("#"), QStringLiteral("Stage"),
       QStringLiteral("Changes"), QStringLiteral("Bytes"),
       QStringLiteral("bit/s"), QStringLiteral("Score"),
       QStringLiteral("Outcome")});
  // The short columns fit their contents; the changes take the rest.
  QHeaderView* header = history_->horizontalHeader();
  header->setSectionResizeMode(QHeaderView::ResizeToContents);
  header->setSectionResizeMode(kChangesColumn, QHeaderView::Stretch);
  header->setSectionResizeMode(kOutcomeColumn, QHeaderView::Interactive);
  history_->verticalHeader()->hide();
  history_->verticalHeader()->setDefaultSectionSize(
      history_->fontMetrics().height() + kRowPadding);
  history_->setWordWrap(false);
  history_->setEditTriggers(QAbstractItemView::NoEditTriggers);
  history_->setVisible(false);
  connect(show_history_, &QCheckBox::toggled, history_,
          &QTableWidget::setVisible);

  auto* limits = new QFormLayout;
  auto* limit_row = new QHBoxLayout;
  limit_row->addWidget(limit_box_);
  limit_row->addWidget(limit_);
  limit_row->addStretch();
  auto* weight_row = new QHBoxLayout;
  weight_row->addWidget(weight_box_);
  weight_row->addWidget(weight_);
  weight_row->addStretch();
  limits->addRow(limit_row);
  limits->addRow(weight_row);
  auto* listen = new QHBoxLayout(listen_row_);
  listen->setContentsMargins(0, 0, 0, 0);
  listen->addWidget(new QLabel(QStringLiteral("Listen:"), listen_row_));
  listen->addWidget(listen_phrase_);
  listen->addWidget(play_original_);
  listen->addWidget(play_optimised_);
  listen->addWidget(listen_note_, 1);
  listen_phrase_->setToolTip(QStringLiteral("The phrase to listen to"));
  auto* buttons = new QHBoxLayout;
  buttons->addStretch();
  for (QPushButton* button : {start_, cancel_, accept_, revert_, close_}) {
    button->setAutoDefault(false);
    buttons->addWidget(button);
  }
  auto* layout = new QVBoxLayout(this);
  layout->addLayout(limits);
  layout->addWidget(status_);
  layout->addWidget(progress_);
  layout->addWidget(quality_);
  layout->addWidget(best_);
  layout->addWidget(listen_row_);
  layout->addWidget(show_history_);
  layout->addWidget(history_, 1);
  layout->addStretch();
  layout->addLayout(buttons);

  connect(start_, &QPushButton::clicked, this, &OptimiseDialog::StartSearch);
  connect(cancel_, &QPushButton::clicked, this, [this] { session_->Cancel(); });
  connect(accept_, &QPushButton::clicked, this, &OptimiseDialog::OnAccept);
  connect(revert_, &QPushButton::clicked, this, &OptimiseDialog::reject);
  connect(close_, &QPushButton::clicked, this, &OptimiseDialog::reject);
  connect(session_, &OptimiseSession::ProgressChanged, this,
          &OptimiseDialog::OnProgress);
  connect(session_, &OptimiseSession::HistoryAdded, this,
          &OptimiseDialog::OnHistoryAdded);
  connect(session_, &OptimiseSession::Finished, this,
          &OptimiseDialog::OnFinished);
  connect(play_original_, &QToolButton::clicked, this,
          [this] { PlayPause(PlaybackSource::kBeforeOptimise); });
  connect(play_optimised_, &QToolButton::clicked, this,
          [this] { PlayPause(PlaybackSource::kOptimised); });
  connect(listen_phrase_, &QComboBox::currentIndexChanged, this, [this] {
    StopListening();
    SpeakListened();
  });
  connect(player_, &Player::StateChanged, this,
          &OptimiseDialog::UpdateListenButtons);
  SetStage(Stage::kReady);
  UpdateListenButtons();
}

OptimiseDialog::~OptimiseDialog() {
  // Speakings post back to this window.
  runner_->Wait();
  disconnect(player_, nullptr, this, nullptr);
  StopListening();
}

void OptimiseDialog::Prepare(const project::Project& project,
                             std::vector<project::PhraseId> ids) {
  project_ = project;
  ids_ = std::move(ids);
  StopListening();
  ++generation_;
  comparisons_.clear();
  listen_phrase_->clear();
  history_->setRowCount(0);
  // The phrase column says nothing when there is one phrase.
  history_->setColumnHidden(kPhraseColumn, ids_.size() == 1);
  quality_->SetPlaceholder(QString());
  best_->clear();
  progress_->setValue(0);
  // The limit starts at the largest of the phrases' sizes now.
  int largest = 0;
  QString name;
  for (const project::PhraseId& id : ids_) {
    const project::Phrase* phrase = project.FindPhrase(id);
    if (phrase == nullptr) {
      continue;
    }
    name = QString::fromStdString(phrase->name);
    if (phrase->encoding.has_value()) {
      largest = std::max(
          largest, core::lpc::SummariseFrames(phrase->encoding->frames).bytes);
    }
  }
  limit_->setValue(largest > 0 ? largest : kMaxLimit);
  setWindowTitle(ids_.size() == 1
                     ? QStringLiteral("Optimise %1").arg(name)
                     : QStringLiteral("Optimise %1 Phrases").arg(ids_.size()));
  status_->setText(QStringLiteral(
      "Searches the encoder settings for the best quality within the "
      "project's PHROM budget. Nothing changes until you accept."));
  SetStage(Stage::kReady);
}

void OptimiseDialog::StartSearch() {
  if (!project_.has_value()) {
    return;
  }
  const project::Project& project = *project_;
  optimiser::OptimiseLimits limits;
  if (limit_box_->isChecked()) {
    limits.max_bytes = limit_->value();
  }
  if (weight_box_->isChecked()) {
    limits.bit_rate_weight = weight_->value();
  }
  history_->setRowCount(0);
  StopListening();
  ++generation_;
  comparisons_.clear();
  listen_phrase_->clear();
  SetStage(Stage::kRunning);
  status_->setText(QStringLiteral("Starting..."));
  if (const core::Status status = session_->Start(project, ids_, limits);
      !status.IsOk()) {
    status_->setText(ToQString(status.Message()));
    SetStage(Stage::kReady);
  }
}

void OptimiseDialog::reject() {
  StopListening();
  if (session_->IsRunning()) {
    // Finished() follows, and discards the results.
    session_->Cancel();
  }
  session_->Discard();
  QDialog::reject();
}

void OptimiseDialog::SetStage(Stage stage) {
  const bool ready = stage == Stage::kReady;
  limit_box_->setEnabled(ready);
  limit_->setEnabled(ready && limit_box_->isChecked());
  weight_box_->setEnabled(ready);
  weight_->setEnabled(ready && weight_box_->isChecked());
  start_->setVisible(ready);
  cancel_->setVisible(stage == Stage::kRunning);
  accept_->setVisible(stage == Stage::kDone);
  revert_->setVisible(stage == Stage::kDone);
  close_->setVisible(ready || stage == Stage::kNothing);
  progress_->setVisible(!ready);
  // There is something to compare only once a search found it.
  listen_row_->setVisible(stage == Stage::kDone);
  if (stage == Stage::kDone) {
    accept_->setDefault(true);
  } else if (ready) {
    start_->setDefault(true);
  }
}

void OptimiseDialog::OnProgress() {
  const std::vector<PhraseOptimisation>& phrases = session_->Phrases();
  const int current = session_->CurrentPhrase();
  if (phrases.empty() || std::cmp_greater_equal(current, phrases.size())) {
    return;
  }
  const PhraseOptimisation& phrase = phrases[static_cast<std::size_t>(current)];
  const int expected = std::max(session_->Expected(), 1);
  progress_->setRange(0, kStepsPerPhrase * static_cast<int>(phrases.size()));
  progress_->setValue((current * kStepsPerPhrase) +
                      (session_->Evaluated() * kStepsPerPhrase / expected));
  status_->setText(QStringLiteral("Phrase %1 of %2 (%3): candidate %4 of at "
                                  "most %5")
                       .arg(current + 1)
                       .arg(phrases.size())
                       .arg(phrase.name)
                       .arg(session_->Evaluated())
                       .arg(session_->Expected()));
  if (phrase.best.has_value()) {
    const std::optional<double> start =
        phrase.start.has_value() && phrase.start->IsScored()
            ? std::optional<double>(phrase.start->score)
            : std::nullopt;
    quality_->SetScore(phrase.best->score, start);
    QString text = QStringLiteral("Best so far: %1, %2 bytes")
                       .arg(QualityText(phrase.best->score))
                       .arg(phrase.best->bytes);
    if (phrase.start.has_value()) {
      text += QStringLiteral(" (start %1, %2 bytes)")
                  .arg(start.has_value()
                           ? QString::number(*start, 'f', 2)
                           : ToQString(optimiser::CandidateOutcomeName(
                                 phrase.start->outcome)))
                  .arg(phrase.start->bytes);
      text += QStringLiteral(" · ") +
              ChangesText(phrase.start->options, phrase.best->options);
    }
    best_->setText(text);
  }
}

void OptimiseDialog::OnHistoryAdded(int first) {
  const std::vector<OptimiseHistoryRow>& rows = session_->History();
  const std::vector<PhraseOptimisation>& phrases = session_->Phrases();
  for (auto r = static_cast<std::size_t>(first); r < rows.size(); ++r) {
    const OptimiseHistoryRow& row = rows[r];
    const optimiser::Candidate& c = row.candidate;
    const PhraseOptimisation& phrase =
        phrases[static_cast<std::size_t>(row.phrase)];
    const int at = history_->rowCount();
    history_->insertRow(at);
    const QString changes = phrase.start.has_value()
                                ? ChangesText(phrase.start->options, c.options)
                                : QString();
    const QStringList cells = {
        phrase.name,
        QString::number(c.index),
        ToQString(optimiser::SearchStageName(c.stage)),
        changes,
        c.bytes > 0 ? QString::number(c.bytes) : QString(),
        c.bytes > 0 ? QString::number(std::lround(c.bit_rate)) : QString(),
        c.IsScored() ? QString::number(c.score, 'f', 3) : QString(),
        c.message.empty()
            ? ToQString(optimiser::CandidateOutcomeName(c.outcome))
            : QStringLiteral("%1: %2").arg(
                  ToQString(optimiser::CandidateOutcomeName(c.outcome)),
                  ToQString(c.message)),
    };
    for (int column = 0; column < kColumnCount; ++column) {
      history_->setItem(at, column, new QTableWidgetItem(cells.at(column)));
    }
  }
}

void OptimiseDialog::OnFinished() {
  progress_->setValue(progress_->maximum());
  if (session_->WasCancelled()) {
    status_->setText(QStringLiteral("Cancelled: nothing was changed."));
    SetStage(Stage::kNothing);
    return;
  }
  QStringList problems;
  for (const PhraseOptimisation& phrase : session_->Phrases()) {
    if (!phrase.problem.isEmpty()) {
      problems << QStringLiteral("%1: %2").arg(phrase.name, phrase.problem);
    }
  }
  const int changes = session_->ChangeCount();
  QString text =
      changes == 0
          ? QStringLiteral("No better settings were found within the limits.")
          : QStringLiteral(
                "Better settings found for %1 of %2 phrase(s). "
                "Accept to keep them, or Revert to keep the "
                "current settings.")
                .arg(changes)
                .arg(session_->Phrases().size());
  if (!problems.isEmpty()) {
    text += QLatin1Char('\n') + problems.join(QLatin1Char('\n'));
  }
  status_->setText(text);
  SetStage(changes == 0 ? Stage::kNothing : Stage::kDone);
  if (changes > 0) {
    PrepareListening();
  }
}

void OptimiseDialog::OnAccept() {
  StopListening();
  if (const core::Status status = session_->Accept(); !status.IsOk()) {
    const QString message = ToQString(status.Message());
    status_->setText(message);
    emit Failed(QStringLiteral("Optimise"), message);
    SetStage(Stage::kNothing);
    return;
  }
  accept();
}

void OptimiseDialog::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange) {
    UpdateListenButtons();
  }
  QDialog::changeEvent(event);
}

bool OptimiseDialog::ShowsPause(PlaybackSource source) const {
  return source == PlaybackSource::kOptimised ? optimised_shows_pause_
                                              : original_shows_pause_;
}

void OptimiseDialog::PrepareListening() {
  const std::vector<PhraseOptimisation>& phrases = session_->Phrases();
  // Filled without signalling, so the first is spoken once, below.
  listen_phrase_->blockSignals(true);
  listen_phrase_->clear();
  for (std::size_t i = 0; i < phrases.size(); ++i) {
    if (phrases[i].change.has_value()) {
      listen_phrase_->addItem(phrases[i].name, static_cast<int>(i));
    }
  }
  listen_phrase_->blockSignals(false);
  listen_phrase_->setVisible(listen_phrase_->count() > 1);
  SpeakListened();
}

int OptimiseDialog::ListenedPhrase() const {
  if (listen_phrase_->currentIndex() < 0) {
    return -1;
  }
  return listen_phrase_->currentData().toInt();
}

void OptimiseDialog::SpeakListened() {
  const int phrase = ListenedPhrase();
  if (phrase < 0 || comparisons_.contains(phrase) || !project_.has_value()) {
    UpdateListenButtons();
    return;
  }
  const PhraseOptimisation& result =
      session_->Phrases()[static_cast<std::size_t>(phrase)];
  const project::Phrase* before = project_->FindPhrase(result.id);
  if (before == nullptr || !result.change.has_value() ||
      !result.change->encoding.has_value()) {
    UpdateListenButtons();
    return;
  }
  Comparison& comparison = comparisons_[phrase];
  comparison.had_frames =
      before->encoding.has_value() && !before->encoding->frames.empty();
  if (comparison.had_frames) {
    Speak(phrase, false, before->audio, before->encoding->frames);
  }
  Speak(phrase, true, before->audio, result.change->encoding->frames);
  UpdateListenButtons();
}

void OptimiseDialog::Speak(int phrase, bool optimised, std::vector<float> audio,
                           std::vector<core::lpc::Frame> frames) {
  ++renders_running_;
  runner_->Post([this, generation = generation_, phrase, optimised,
                 audio = std::move(audio), frames = std::move(frames),
                 synth = synth_] {
    auto result = std::make_shared<core::StatusOr<quality::EncodedAudio>>(
        quality::RenderEncodedAudio(*synth, audio, frames));
    QMetaObject::invokeMethod(
        this,
        [this, generation, phrase, optimised, result] {
          OnSpoken(generation, phrase, optimised, std::move(*result));
        },
        Qt::QueuedConnection);
  });
}

void OptimiseDialog::OnSpoken(int generation, int phrase, bool optimised,
                              core::StatusOr<quality::EncodedAudio> result) {
  --renders_running_;
  if (generation != generation_) {
    return;
  }
  const auto found = comparisons_.find(phrase);
  if (found == comparisons_.end()) {
    return;
  }
  Comparison& comparison = found->second;
  if (!result.IsOk()) {
    core::Logger(core::kLogModuleGui)
        ->warn("cannot speak the frames: {}", result.GetStatus().Message());
    comparison.problem = QStringLiteral("Cannot speak the frames: %1")
                             .arg(ToQString(result.GetStatus().Message()));
  } else {
    (optimised ? comparison.optimised : comparison.original) =
        std::move(*result);
  }
  UpdateListenButtons();
}

void OptimiseDialog::PlayPause(PlaybackSource source) {
  if (source != PlaybackSource::kBeforeOptimise &&
      source != PlaybackSource::kOptimised) {
    return;
  }
  if (player_->State() != PlaybackState::kStopped &&
      player_->Source() == source) {
    if (player_->State() == PlaybackState::kPlaying) {
      player_->Pause();
    } else if (const core::Status status = player_->Resume(); !status.IsOk()) {
      listen_note_->setText(ToQString(status.Message()));
    }
    return;
  }
  const auto found = comparisons_.find(ListenedPhrase());
  if (found == comparisons_.end()) {
    return;
  }
  const std::optional<quality::EncodedAudio>& audio =
      source == PlaybackSource::kOptimised ? found->second.optimised
                                           : found->second.original;
  if (!audio.has_value()) {
    return;
  }
  if (const core::Status status =
          player_->Play(source, AtRecordingLevel(*audio, pin_), 0);
      !status.IsOk()) {
    listen_note_->setText(ToQString(status.Message()));
  }
}

void OptimiseDialog::UpdateListenButtons() {
  const bool playing = player_->State() == PlaybackState::kPlaying;
  original_shows_pause_ =
      playing && player_->Source() == PlaybackSource::kBeforeOptimise;
  optimised_shows_pause_ =
      playing && player_->Source() == PlaybackSource::kOptimised;
  play_original_->setIcon(PlayPauseIcon(original_shows_pause_, *this));
  play_optimised_->setIcon(PlayPauseIcon(optimised_shows_pause_, *this));
  play_original_->setToolTip(
      original_shows_pause_
          ? QStringLiteral("Pause the frames before Optimise")
          : QStringLiteral("Play the frames before Optimise, which Revert "
                           "keeps"));
  play_optimised_->setToolTip(
      optimised_shows_pause_
          ? QStringLiteral("Pause the optimised frames")
          : QStringLiteral("Play the optimised frames, which Accept keeps"));

  const auto found = comparisons_.find(ListenedPhrase());
  const Comparison* comparison =
      found == comparisons_.end() ? nullptr : &found->second;
  play_original_->setEnabled(comparison != nullptr &&
                             comparison->original.has_value());
  play_optimised_->setEnabled(comparison != nullptr &&
                              comparison->optimised.has_value());
  QString note;
  if (comparison != nullptr) {
    if (!comparison->problem.isEmpty()) {
      note = comparison->problem;
    } else if (!comparison->optimised.has_value() ||
               (comparison->had_frames && !comparison->original.has_value())) {
      note = QStringLiteral("Speaking the frames...");
    } else if (!comparison->had_frames) {
      note = QStringLiteral("The phrase had no frames before Optimise");
    }
  }
  listen_note_->setText(note);
}

void OptimiseDialog::StopListening() {
  if (player_->State() != PlaybackState::kStopped &&
      (player_->Source() == PlaybackSource::kBeforeOptimise ||
       player_->Source() == PlaybackSource::kOptimised)) {
    player_->Stop();
  }
}

}  // namespace speak5220::app
