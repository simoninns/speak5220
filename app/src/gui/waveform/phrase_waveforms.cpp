/*
 * File:        phrase_waveforms.cpp
 * Module:      waveform
 * Purpose:     The phrase editor's two waveform panes, original and
 *              encoded: rendering, the shared time axis and controls, the
 *              play-start marker and playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/phrase_waveforms.h"

#include <QCheckBox>
#include <QEvent>
#include <QHBoxLayout>
#include <QIcon>
#include <QMetaObject>
#include <QScrollBar>
#include <QString>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "core/dsp/squelch.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/synth.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/playback/playback_icon.h"
#include "gui/playback/player.h"
#include "gui/undo/project_edits.h"
#include "gui/waveform/time_axis.h"
#include "gui/waveform/waveform_pane.h"
#include "gui/waveform/waveform_view.h"

namespace speak5220::app {

namespace project = core::project;
namespace quality = core::quality;
namespace lpc = core::lpc;

namespace {

QToolButton* MakeButton(const QString& text, const QString& tip,
                        QWidget* parent) {
  auto* button = new QToolButton(parent);
  button->setText(text);
  button->setToolTip(tip);
  button->setAccessibleName(tip);
  button->setAutoRaise(true);
  return button;
}

// A play/pause button labelled with its pane's name beside the icon.
QToolButton* MakePlayButton(const QString& label, QWidget* parent) {
  auto* button = new QToolButton(parent);
  button->setText(label);
  button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
  button->setAutoRaise(true);
  return button;
}

// Full scale of a 16-bit sample.
constexpr double kFullScale = 32768.0;
// Decibels per decade of amplitude.
constexpr double kDecibelsPerDecade = 20.0;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

// `samples` as fractions of full scale, times `gain`.
std::vector<float> ToFloat(const std::vector<std::int16_t>& samples,
                           double gain) {
  std::vector<float> out;
  out.reserve(samples.size());
  for (const std::int16_t v : samples) {
    out.push_back(static_cast<float>(v * gain / kFullScale));
  }
  return out;
}

}  // namespace

PhraseWaveforms::PhraseWaveforms(ProjectEdits& edits, Player& player,
                                 core::synth::ISynth& synth,
                                 core::ITaskRunner& runner, QWidget* parent)
    : QWidget(parent),
      edits_(&edits),
      player_(&player),
      synth_(&synth),
      runner_(&runner),
      original_(new WaveformPane(QStringLiteral("Original (8 kHz)"), this)),
      encoded_(new WaveformPane(QStringLiteral("Encoded"), this)),
      scroll_bar_(new QScrollBar(Qt::Horizontal, this)),
      play_original_(MakePlayButton(QStringLiteral("Original"), this)),
      play_encoded_(MakePlayButton(QStringLiteral("Encoded"), this)),
      reset_(MakeButton(QStringLiteral("|◀"),
                        QStringLiteral("Move the play start to the beginning"),
                        this)),
      grid_(new QCheckBox(QStringLiteral("Frame &grid"), this)),
      zoom_out_(
          MakeButton(QStringLiteral("−"), QStringLiteral("Zoom out"), this)),
      zoom_in_(
          MakeButton(QStringLiteral("+"), QStringLiteral("Zoom in"), this)),
      fit_(MakeButton(QStringLiteral("Fit"),
                      QStringLiteral("Show the whole phrase"), this)) {
  grid_->setChecked(true);
  grid_->setToolTip(
      QStringLiteral("Show the 25 ms frames on the encoded pane"));

  SetPlaying(PlaybackSource::kOriginal, false);
  SetPlaying(PlaybackSource::kEncoded, false);
  auto* controls = new QHBoxLayout;
  controls->addWidget(play_original_);
  controls->addWidget(play_encoded_);
  controls->addWidget(reset_);
  controls->addStretch();
  controls->addWidget(grid_);
  controls->addWidget(zoom_out_);
  controls->addWidget(zoom_in_);
  controls->addWidget(fit_);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  // The two views share the height equally.
  layout->addWidget(original_, 1);
  layout->addWidget(encoded_, 1);
  layout->addWidget(scroll_bar_);
  layout->addLayout(controls);

  WaveformView* original_view = original_->View();
  WaveformView* encoded_view = encoded_->View();
  connect(grid_, &QCheckBox::toggled, encoded_view,
          &WaveformView::SetFrameGridShown);
  connect(original_view, &WaveformView::AxisChanged, this,
          [this, original_view, encoded_view] {
            FollowAxis(original_view, encoded_view);
            UpdateScrollBar();
          });
  connect(encoded_view, &WaveformView::AxisChanged, this,
          [this, original_view, encoded_view] {
            FollowAxis(encoded_view, original_view);
            UpdateScrollBar();
          });
  for (WaveformView* view : {original_view, encoded_view}) {
    connect(view, &WaveformView::MarkerRequested, this,
            &PhraseWaveforms::MovePlayStart);
  }
  connect(zoom_out_, &QToolButton::clicked, this,
          [this] { LeadView()->ZoomOut(); });
  connect(zoom_in_, &QToolButton::clicked, this,
          [this] { LeadView()->ZoomIn(); });
  connect(fit_, &QToolButton::clicked, this, [this] { LeadView()->Fit(); });
  connect(scroll_bar_, &QScrollBar::valueChanged, this,
          &PhraseWaveforms::OnScrolled);
  connect(reset_, &QToolButton::clicked, this, [this] { MovePlayStart(0.0); });
  connect(play_original_, &QToolButton::clicked, this,
          [this] { PlayPause(PlaybackSource::kOriginal); });
  connect(play_encoded_, &QToolButton::clicked, this,
          [this] { PlayPause(PlaybackSource::kEncoded); });
  connect(original_view, &WaveformView::PlayPauseRequested, this,
          [this] { PlayPause(PlaybackSource::kOriginal); });
  connect(encoded_view, &WaveformView::PlayPauseRequested, this,
          [this] { PlayPause(PlaybackSource::kEncoded); });
  connect(player_, &Player::StateChanged, this,
          &PhraseWaveforms::OnPlayerStateChanged);
  connect(player_, &Player::PositionChanged, this,
          &PhraseWaveforms::OnPlayerPositionChanged);
  ShowPhrase(nullptr);
}

PhraseWaveforms::~PhraseWaveforms() {
  // Renders post back to this widget.
  runner_->Wait();
  disconnect(player_, nullptr, this, nullptr);
  player_->Stop();
}

void PhraseWaveforms::ShowPhrase(const project::Phrase* phrase,
                                 const QString& encoded_note) {
  encoded_note_ = encoded_note;
  UpdateEncodedNote();
  if (phrase == nullptr) {
    player_->Stop();
    id_.clear();
    audio_.clear();
    frames_.clear();
    encoded_shown_ = false;
    const QString none = QStringLiteral("Select one phrase to see it");
    original_->View()->Clear(none);
    encoded_->View()->Clear(none);
    UpdateShading();
    play_original_->setEnabled(false);
    play_encoded_->setEnabled(false);
    reset_->setEnabled(false);
    UpdateEncodedNote();
    UpdateExtents();
    return;
  }

  const bool same_phrase = phrase->id == id_;
  if (!same_phrase) {
    player_->Stop();
    id_ = phrase->id;
    // A phrase's audio never changes once imported (plan §3.1).
    audio_ = phrase->audio;
    if (audio_.empty()) {
      original_->View()->Clear(
          QStringLiteral("No source audio: the phrase is made of frames "
                         "alone"));
    } else {
      // Not followed: UpdateExtents() fits both panes together.
      following_ = true;
      original_->View()->SetSamples(audio_, 0);
      following_ = false;
    }
    UpdateShading();
    play_original_->setEnabled(!audio_.empty());
    reset_->setEnabled(!audio_.empty());
    frames_.clear();
    encoded_shown_ = false;
  }
  play_start_ = phrase->play_start;
  const std::optional<int> marker =
      audio_.empty() ? std::nullopt : std::optional<int>(play_start_);
  original_->View()->SetMarker(marker);
  encoded_->View()->SetMarker(marker);

  std::vector<lpc::Frame> frames;
  if (phrase->encoding.has_value()) {
    frames = phrase->encoding->frames;
  }
  if (frames.empty()) {
    frames_.clear();
    encoded_shown_ = false;
    UpdateEncodedNote();
    encoded_->View()->Clear(
        QStringLiteral("Not encoded yet: press Encode below"));
    play_encoded_->setEnabled(false);
  } else if (!same_phrase || frames != frames_) {
    frames_ = std::move(frames);
    encoded_shown_ = false;
    const auto found = rendered_.find(id_);
    if (found != rendered_.end() && found->second.frames == frames_) {
      ShowEncoded(found->second);
      UpdateExtents();
    } else {
      encoded_->View()->Clear(QStringLiteral("Speaking the frames..."));
      play_encoded_->setEnabled(false);
      Render(frames_);
    }
  }
  UpdateEncodedNote();
  if (!same_phrase) {
    UpdateExtents();
  }
}

void PhraseWaveforms::PlayPause(PlaybackSource source) {
  if (id_.empty()) {
    return;
  }
  if (player_->State() != PlaybackState::kStopped &&
      player_->Source() == source) {
    if (player_->State() == PlaybackState::kPlaying) {
      player_->Pause();
    } else if (const core::Status status = player_->Resume(); !status.IsOk()) {
      emit Failed(QStringLiteral("Play"), ToQString(status.Message()));
    }
    return;
  }
  core::Status status = core::OkStatus();
  if (source == PlaybackSource::kOriginal) {
    status = player_->Play(source, ToPcm16(audio_), play_start_);
  } else if (encoded_shown_) {
    // The same sound as the play start, or the beginning for a phrase
    // with no source audio.
    const int count = static_cast<int>(encoded_audio_.samples.size());
    const int start =
        audio_.empty()
            ? 0
            : std::clamp(play_start_ + encoded_audio_.source_offset, 0, count);
    status = player_->Play(source, encoded_playback_, start);
  } else {
    return;
  }
  if (!status.IsOk()) {
    emit Failed(QStringLiteral("Play"), ToQString(status.Message()));
  }
}

void PhraseWaveforms::MovePlayStart(double source_sample) {
  if (id_.empty() || audio_.empty()) {
    return;
  }
  const int sample = static_cast<int>(std::clamp(
      std::round(source_sample), 0.0, static_cast<double>(audio_.size())));
  if (sample != play_start_) {
    if (const core::Status status = edits_->SetPlayStart(id_, sample);
        !status.IsOk()) {
      emit Failed(QStringLiteral("Move Play Start"),
                  ToQString(status.Message()));
      return;
    }
    play_start_ = sample;
    original_->View()->SetMarker(sample);
    encoded_->View()->SetMarker(sample);
  }
  FollowPlayStart();
}

void PhraseWaveforms::FollowPlayStart() {
  const PlaybackState state = player_->State();
  if (state == PlaybackState::kStopped) {
    return;
  }
  const PlaybackSource source = player_->Source();
  player_->Stop();
  if (state == PlaybackState::kPlaying) {
    PlayPause(source);
  }
}

void PhraseWaveforms::Render(const std::vector<lpc::Frame>& frames) {
  ++renders_running_;
  runner_->Post([this, id = id_, audio = audio_, frames, synth = synth_] {
    auto result = std::make_shared<core::StatusOr<quality::EncodedAudio>>(
        quality::RenderEncodedAudio(*synth, audio, frames));
    QMetaObject::invokeMethod(
        this,
        [this, id, frames, result] {
          OnRendered(id, frames, std::move(*result));
        },
        Qt::QueuedConnection);
  });
}

void PhraseWaveforms::OnRendered(const project::PhraseId& id,
                                 std::vector<lpc::Frame> frames,
                                 core::StatusOr<quality::EncodedAudio> result) {
  --renders_running_;
  const bool current = id == id_ && frames == frames_;
  if (!result.IsOk()) {
    core::Logger(core::kLogModuleGui)
        ->warn("cannot speak the frames: {}", result.GetStatus().Message());
    if (current) {
      encoded_->View()->Clear(
          QStringLiteral("Cannot speak the frames: %1")
              .arg(ToQString(result.GetStatus().Message())));
    }
    emit RenderFinished();
    return;
  }
  Rendered& rendered = rendered_[id];
  rendered = {.frames = std::move(frames), .audio = std::move(*result)};
  if (current) {
    ShowEncoded(rendered);
    UpdateExtents();
  }
  emit RenderFinished();
}

void PhraseWaveforms::ShowEncoded(const Rendered& rendered) {
  encoded_audio_ = rendered.audio;
  encoded_shown_ = true;
  // Shown and played at the original's level, so the two compare like
  // with like; the frames and exports are unchanged.
  std::vector<float> matched =
      ToFloat(encoded_audio_.samples, encoded_audio_.level_gain);
  encoded_playback_ = ToPcm16(matched);
  WaveformView* view = encoded_->View();
  // Not followed: its own fit would be copied to the original, which
  // would then no longer be fitted. UpdateExtents() fits both together.
  following_ = true;
  view->SetSamples(std::move(matched), encoded_audio_.source_offset);
  following_ = false;
  UpdateEncodedNote();
  view->SetFrames(rendered.frames);
  view->SetFrameGridShown(grid_->isChecked());
  play_encoded_->setEnabled(!encoded_audio_.samples.empty());
}

void PhraseWaveforms::UpdateExtents() {
  WaveformView* original_view = original_->View();
  WaveformView* encoded_view = encoded_->View();
  TimeExtent original_extent = original_view->ContentExtent();
  TimeExtent encoded_extent = encoded_view->ContentExtent();
  if (original_view->HasSamples() && encoded_view->HasSamples()) {
    const TimeExtent both{
        .begin = std::min(original_extent.begin, encoded_extent.begin),
        .end = std::max(original_extent.end, encoded_extent.end)};
    original_extent = both;
    encoded_extent = both;
  }
  following_ = true;
  original_view->SetExtent(original_extent);
  encoded_view->SetExtent(encoded_extent);
  following_ = false;
  // New contents start fitted: the whole of both, as Fit shows them.
  LeadView()->Fit();
  FollowAxis(LeadView(),
             LeadView() == original_view ? encoded_view : original_view);
  UpdateScrollBar();
}

void PhraseWaveforms::FollowAxis(WaveformView* from, WaveformView* to) {
  if (following_ || !from->HasSamples() || !to->HasSamples()) {
    return;
  }
  following_ = true;
  to->SetAxis(from->Axis());
  following_ = false;
}

WaveformView* PhraseWaveforms::LeadView() const {
  return original_->View()->HasSamples() ? original_->View() : encoded_->View();
}

void PhraseWaveforms::UpdateScrollBar() {
  // The scroll bar counts pixels from the extent's start.
  const WaveformView* view = LeadView();
  const TimeAxis& axis = view->Axis();
  const TimeExtent& extent = view->Extent();
  const int page = std::max(view->width(), 1);
  const auto total = static_cast<int>(
      std::lround((extent.end - extent.begin) / axis.samples_per_pixel));
  updating_scroll_bar_ = true;
  scroll_bar_->setRange(0, std::max(total - page, 0));
  scroll_bar_->setPageStep(page);
  scroll_bar_->setValue(static_cast<int>(
      std::lround((axis.first - extent.begin) / axis.samples_per_pixel)));
  updating_scroll_bar_ = false;
}

void PhraseWaveforms::OnScrolled(int value) {
  if (updating_scroll_bar_) {
    return;
  }
  WaveformView* view = LeadView();
  TimeAxis axis = view->Axis();
  axis.first = view->Extent().begin + (value * axis.samples_per_pixel);
  view->SetAxis(axis);
}

void PhraseWaveforms::UpdateEncodedNote() {
  QStringList parts;
  if (encoded_shown_ && encoded_audio_.level_gain != 1.0) {
    parts << QStringLiteral("level matched to the original (%1 dB)")
                 .arg(
                     kDecibelsPerDecade * std::log10(encoded_audio_.level_gain),
                     0, 'f', 1);
  }
  if (!encoded_note_.isEmpty()) {
    parts << encoded_note_;
  }
  encoded_->SetNote(parts.join(QStringLiteral(" · ")));
}

void PhraseWaveforms::SetSquelch(std::optional<double> squelch_db) {
  if (squelch_db != squelch_db_) {
    squelch_db_ = squelch_db;
    UpdateShading();
  }
}

void PhraseWaveforms::UpdateShading() {
  if (audio_.empty() || !squelch_db_.has_value()) {
    original_->View()->SetShading({});
    original_->SetNote(QString());
    return;
  }
  core::encoder::EncoderOptions options;
  options.squelch_db = squelch_db_;
  const std::vector<double> samples(audio_.begin(), audio_.end());
  std::vector<TimeExtent> shading;
  for (const core::dsp::SampleSpan& span :
       core::encoder::SquelchedSpans(samples, options)) {
    shading.push_back({.begin = static_cast<double>(span.begin),
                       .end = static_cast<double>(span.end)});
  }
  const QString level = QString::fromStdString(
      core::encoder::EncoderOptionText(options, "squelch_db"));
  original_->SetNote(
      shading.empty()
          ? QStringLiteral("squelch %1 dB: nothing is quiet enough to mute")
                .arg(level)
          : QStringLiteral("squelch %1 dB: shaded stretches are muted")
                .arg(level));
  original_->View()->SetShading(std::move(shading));
}

QToolButton* PhraseWaveforms::PlayButton(PlaybackSource source) const {
  return source == PlaybackSource::kOriginal ? play_original_ : play_encoded_;
}

bool PhraseWaveforms::ShowsPause(PlaybackSource source) const {
  return source == PlaybackSource::kOriginal ? original_shows_pause_
                                             : encoded_shows_pause_;
}

void PhraseWaveforms::SetPlaying(PlaybackSource source, bool playing) {
  (source == PlaybackSource::kOriginal ? original_shows_pause_
                                       : encoded_shows_pause_) = playing;
  QToolButton* button = PlayButton(source);
  const QString name = source == PlaybackSource::kOriginal
                           ? QStringLiteral("the original")
                           : QStringLiteral("the encoded audio");
  // The label stays and only the icon changes, so the button keeps its
  // width. The icon is drawn in the button text's colour, so it matches
  // the other controls in light and dark (plan §5, "Colours").
  button->setIcon(PlayPauseIcon(playing, *this));
  button->setToolTip(
      (playing ? QStringLiteral("Pause %1") : QStringLiteral("Play %1 (Space)"))
          .arg(name));
  button->setAccessibleName(
      (playing ? QStringLiteral("Pause %1") : QStringLiteral("Play %1"))
          .arg(name));
}

void PhraseWaveforms::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange) {
    SetPlaying(PlaybackSource::kOriginal, original_shows_pause_);
    SetPlaying(PlaybackSource::kEncoded, encoded_shows_pause_);
  }
  QWidget::changeEvent(event);
}

void PhraseWaveforms::OnPlayerStateChanged() {
  const PlaybackState state = player_->State();
  const bool playing = state == PlaybackState::kPlaying;
  SetPlaying(PlaybackSource::kOriginal,
             playing && player_->Source() == PlaybackSource::kOriginal);
  SetPlaying(PlaybackSource::kEncoded,
             playing && player_->Source() == PlaybackSource::kEncoded);
  if (state == PlaybackState::kStopped) {
    original_->View()->SetPlayhead(std::nullopt);
    encoded_->View()->SetPlayhead(std::nullopt);
  }
}

void PhraseWaveforms::OnPlayerPositionChanged(int position) {
  WaveformView* view = PlayingView();
  if (view == nullptr) {
    return;
  }
  // The playhead in source samples, where the views count time.
  view->SetPlayhead(static_cast<double>(position - view->SourceOffset()));
}

WaveformView* PhraseWaveforms::PlayingView() const {
  if (player_->State() == PlaybackState::kStopped) {
    return nullptr;
  }
  switch (player_->Source()) {
    case PlaybackSource::kOriginal:
      return original_->View();
    case PlaybackSource::kEncoded:
      return encoded_->View();
    case PlaybackSource::kBeforeOptimise:
    case PlaybackSource::kOptimised:
      // The Optimise window's audio has no playhead here.
      break;
  }
  return nullptr;
}

}  // namespace speak5220::app
