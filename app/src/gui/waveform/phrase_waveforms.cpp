/*
 * File:        phrase_waveforms.cpp
 * Module:      waveform
 * Purpose:     The phrase editor's tabs, each a waveform with its own
 *              controls: the original, the preprocessed and the encoded
 *              audio; rendering, linked time axes, the play-start marker
 *              and playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/phrase_waveforms.h"

#include <QCheckBox>
#include <QEvent>
#include <QFrame>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QMetaObject>
#include <QPixmap>
#include <QScrollArea>
#include <QScrollBar>
#include <QSplitter>
#include <QStackedWidget>
#include <QString>
#include <QStringList>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "core/dsp/noise_gate.h"
#include "core/dsp/output_filter.h"
#include "core/lpc/frame.h"
#include "core/preprocess/preprocess.h"
#include "core/project/phrase.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/logging.h"
#include "core/util/number_text.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/playback/playback_icon.h"
#include "gui/playback/player.h"
#include "gui/undo/project_edits.h"
#include "gui/waveform/grip_splitter.h"
#include "gui/waveform/phrase_tab.h"
#include "gui/waveform/time_axis.h"
#include "gui/waveform/waveform_pane.h"
#include "gui/waveform/waveform_view.h"

namespace speak5220::app {

namespace project = core::project;
namespace quality = core::quality;
namespace lpc = core::lpc;

namespace {

// The share of the height the waveform area starts with, against the
// settings area's.
constexpr int kWaveformStretch = 3;
constexpr int kSettingsStretch = 2;

// Full scale of a 16-bit sample.
constexpr double kFullScale = 32768.0;
// Decibels per decade of amplitude.
constexpr double kDecibelsPerDecade = 20.0;
// The logo shown with no phrase (app/resources/speak5220.qrc), drawn at
// twice its size in pixels for high-density screens.
constexpr const char* kLogoPath = ":/speak5220/speak5220-logo@2x.png";
constexpr double kLogoPixelRatio = 2.0;

QToolButton* MakeButton(const QString& text, const QString& tip,
                        QWidget* parent) {
  auto* button = new QToolButton(parent);
  button->setText(text);
  button->setToolTip(tip);
  button->setAccessibleName(tip);
  button->setAutoRaise(true);
  return button;
}

// The preprocessing after the noise gate in words, for the preprocessed
// waveform's note: "bass cut 200 Hz, treble tilt 0.4, de-esser 10 dB". The
// noise gate has a note of its own.
QString DescribeAfterGate(const core::preprocess::PreprocessOptions& options) {
  QStringList parts;
  if (options.bass_cut_hz.has_value()) {
    parts << QStringLiteral("bass cut %1 Hz").arg(*options.bass_cut_hz);
  }
  if (options.treble_tilt > 0.0) {
    parts << QStringLiteral("treble tilt %1").arg(options.treble_tilt);
  }
  if (options.de_ess_db.has_value()) {
    parts << QStringLiteral("de-esser %1 dB").arg(*options.de_ess_db);
  }
  return parts.join(QStringLiteral(", "));
}

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

// What a tab's play button plays, in its tooltip: "the original".
QString AudioName(PlaybackSource source) {
  switch (source) {
    case PlaybackSource::kOriginal:
      return QStringLiteral("the original");
    case PlaybackSource::kPreprocessed:
      return QStringLiteral("the preprocessed recording");
    case PlaybackSource::kEncoded:
    case PlaybackSource::kBeforeOptimise:
    case PlaybackSource::kOptimised:
      break;
  }
  return QStringLiteral("the encoded audio");
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
      tab_bar_(new QTabBar(this)),
      splitter_(new GripSplitter(Qt::Vertical, this)),
      waveform_stack_(new QStackedWidget(splitter_)),
      settings_stack_(new QStackedWidget(splitter_)) {
  constexpr std::array<PlaybackSource, 3> kSources = {
      PlaybackSource::kOriginal, PlaybackSource::kPreprocessed,
      PlaybackSource::kEncoded};
  for (std::size_t i = 0; i < tracks_.size(); ++i) {
    tracks_[i].tab = kPhraseTabs[i];
    tracks_[i].source = kSources[i];
  }
  BuildTrack(TrackOf(PhraseTab::kOriginal), QStringLiteral("Original"),
             QStringLiteral("Original (8 kHz)"), QStringLiteral("original"));
  BuildTrack(TrackOf(PhraseTab::kPreprocess), QStringLiteral("Preprocess"),
             QStringLiteral("Preprocessed (8 kHz)"),
             QStringLiteral("preprocessed recording"));
  BuildTrack(TrackOf(PhraseTab::kEncode), QStringLiteral("Encode"),
             QStringLiteral("Encoded"), QStringLiteral("encoded audio"));
  // GripSplitter, so its handle can be seen in every style.
  splitter_->addWidget(waveform_stack_);
  splitter_->addWidget(settings_stack_);
  splitter_->setStretchFactor(0, kWaveformStretch);
  splitter_->setStretchFactor(1, kSettingsStretch);
  splitter_->setChildrenCollapsible(false);
  tab_bar_->setExpanding(false);
  tab_bar_->setDrawBase(false);
  auto* layout = new QVBoxLayout(this);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(tab_bar_);
  layout->addWidget(splitter_, 1);

  connect(tab_bar_, &QTabBar::currentChanged, this, [this](int index) {
    waveform_stack_->setCurrentIndex(index);
    settings_stack_->setCurrentIndex(index);
    emit CurrentTabChanged(CurrentTab());
  });
  connect(player_, &Player::StateChanged, this,
          &PhraseWaveforms::OnPlayerStateChanged);
  connect(player_, &Player::PositionChanged, this,
          &PhraseWaveforms::OnPlayerPositionChanged);
  logo_ = QPixmap(QString::fromLatin1(kLogoPath));
  logo_.setDevicePixelRatio(kLogoPixelRatio);
  ShowPhrase(nullptr);
}

PhraseWaveforms::~PhraseWaveforms() {
  // Renders post back to this widget.
  runner_->Wait();
  disconnect(player_, nullptr, this, nullptr);
  player_->Stop();
}

void PhraseWaveforms::BuildTrack(Track& track, const QString& tab_title,
                                 const QString& pane_title,
                                 const QString& audio_name) {
  // The waveform area's page: the waveform and its controls.
  auto* top = new QWidget(waveform_stack_);
  track.pane = new WaveformPane(pane_title, top);
  track.scroll_bar = new QScrollBar(Qt::Horizontal, top);
  track.play = new QToolButton(top);
  track.play->setAutoRaise(true);
  track.reset =
      MakeButton(QStringLiteral("|◀"),
                 QStringLiteral("Move the play start to the beginning"), top);
  track.zoom_out =
      MakeButton(QStringLiteral("−"),
                 QStringLiteral("Zoom out the %1").arg(audio_name), top);
  track.zoom_in =
      MakeButton(QStringLiteral("+"),
                 QStringLiteral("Zoom in the %1").arg(audio_name), top);
  track.fit = MakeButton(QStringLiteral("Fit"),
                         QStringLiteral("Show the whole phrase"), top);

  auto* controls = new QHBoxLayout;
  controls->addWidget(track.play);
  controls->addWidget(track.reset);
  controls->addStretch();
  if (track.tab == PhraseTab::kEncode) {
    grid_ = new QCheckBox(QStringLiteral("Frame &grid"), top);
    grid_->setChecked(true);
    grid_->setToolTip(QStringLiteral("Show the 25 ms frames"));
    controls->addWidget(grid_);
    connect(grid_, &QCheckBox::toggled, track.pane->View(),
            &WaveformView::SetFrameGridShown);
  }
  controls->addWidget(track.zoom_out);
  controls->addWidget(track.zoom_in);
  controls->addWidget(track.fit);
  auto* layout = new QVBoxLayout(top);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->addWidget(track.pane, 1);
  layout->addWidget(track.scroll_bar);
  layout->addLayout(controls);
  waveform_stack_->addWidget(top);

  // The settings area's page, which scrolls rather than growing.
  track.settings = new QScrollArea(settings_stack_);
  track.settings->setWidgetResizable(true);
  track.settings->setFrameShape(QFrame::NoFrame);
  auto* none = new QLabel(QStringLiteral("No settings"));
  none->setAlignment(Qt::AlignCenter);
  none->setForegroundRole(QPalette::PlaceholderText);
  track.settings->setWidget(none);
  settings_stack_->addWidget(track.settings);
  tab_bar_->addTab(tab_title);
  SetPlaying(track, false);

  const PhraseTab tab = track.tab;
  const PlaybackSource source = track.source;
  WaveformView* view = track.pane->View();
  connect(view, &WaveformView::AxisChanged, this, [this, view, tab] {
    FollowAxis(view);
    UpdateScrollBar(TrackOf(tab));
  });
  connect(view, &WaveformView::MarkerRequested, this,
          &PhraseWaveforms::MovePlayStart);
  connect(view, &WaveformView::PlayPauseRequested, this,
          [this, source] { PlayPause(source); });
  connect(track.play, &QToolButton::clicked, this,
          [this, source] { PlayPause(source); });
  connect(track.reset, &QToolButton::clicked, this,
          [this] { MovePlayStart(0.0); });
  connect(track.zoom_out, &QToolButton::clicked, view, &WaveformView::ZoomOut);
  connect(track.zoom_in, &QToolButton::clicked, view, &WaveformView::ZoomIn);
  connect(track.fit, &QToolButton::clicked, view, &WaveformView::Fit);
  connect(track.scroll_bar, &QScrollBar::valueChanged, this,
          [this, tab](int value) { OnScrolled(TrackOf(tab), value); });
}

const PhraseWaveforms::Track& PhraseWaveforms::TrackOf(PhraseTab tab) const {
  return tracks_[static_cast<std::size_t>(tab)];
}

PhraseWaveforms::Track& PhraseWaveforms::TrackOf(PhraseTab tab) {
  return tracks_[static_cast<std::size_t>(tab)];
}

const PhraseWaveforms::Track* PhraseWaveforms::TrackFor(
    PlaybackSource source) const {
  for (const Track& track : tracks_) {
    if (track.source == source) {
      return &track;
    }
  }
  return nullptr;
}

WaveformView* PhraseWaveforms::ViewOf(PhraseTab tab) const {
  return TrackOf(tab).pane->View();
}

void PhraseWaveforms::SetControls(PhraseTab tab, QWidget* controls) {
  // Replaces, and deletes, what the area held.
  TrackOf(tab).settings->setWidget(controls);
}

void PhraseWaveforms::SetCurrentTab(PhraseTab tab) {
  tab_bar_->setCurrentIndex(static_cast<int>(tab));
}

PhraseTab PhraseWaveforms::CurrentTab() const {
  const int index = tab_bar_->currentIndex();
  return index < 0 ? PhraseTab::kOriginal
                   : kPhraseTabs[static_cast<std::size_t>(index)];
}

void PhraseWaveforms::ShowPhrase(const project::Phrase* phrase,
                                 const QString& encoded_note) {
  encoded_note_ = encoded_note;
  if (phrase == nullptr) {
    player_->Stop();
    id_.clear();
    audio_.clear();
    UpdatePreprocessed();
    frames_.clear();
    encoded_shown_ = false;
    for (const Track& track : tracks_) {
      track.pane->View()->Clear(QStringLiteral("Select one phrase to see it"),
                                logo_);
    }
    UpdatePreprocessedLabels();
    UpdateEncodedNote();
    UpdateExtents();
    UpdateControls();
    return;
  }

  const bool same_phrase = phrase->id == id_;
  if (!same_phrase) {
    player_->Stop();
    id_ = phrase->id;
    // A phrase's audio never changes once imported (plan §3.1).
    audio_ = phrase->audio;
    UpdatePreprocessed();
    if (audio_.empty()) {
      const QString none =
          QStringLiteral("No source audio: the phrase is made of frames alone");
      ViewOf(PhraseTab::kOriginal)->Clear(none);
      ViewOf(PhraseTab::kPreprocess)->Clear(none);
    } else {
      // Not followed: UpdateExtents() fits them all together.
      following_ = true;
      ViewOf(PhraseTab::kOriginal)->SetSamples(audio_, 0);
      ViewOf(PhraseTab::kPreprocess)->SetSamples(preprocessed_, 0);
      following_ = false;
    }
    UpdatePreprocessedLabels();
    frames_.clear();
    encoded_shown_ = false;
  }
  play_start_ = phrase->play_start;
  const std::optional<int> marker =
      audio_.empty() ? std::nullopt : std::optional<int>(play_start_);
  for (const Track& track : tracks_) {
    track.pane->View()->SetMarker(marker);
  }

  std::vector<lpc::Frame> frames;
  if (phrase->encoding.has_value()) {
    frames = phrase->encoding->frames;
  }
  WaveformView* encoded = ViewOf(PhraseTab::kEncode);
  if (frames.empty()) {
    frames_.clear();
    encoded_shown_ = false;
    encoded->Clear(QStringLiteral("Not encoded yet: press Encode below"));
  } else if (!same_phrase || frames != frames_) {
    frames_ = std::move(frames);
    encoded_shown_ = false;
    const auto found = rendered_.find(id_);
    if (found != rendered_.end() && found->second.frames == frames_) {
      ShowEncoded(found->second);
      UpdateExtents();
    } else {
      encoded->Clear(QStringLiteral("Speaking the frames..."));
      Render(frames_);
    }
  }
  UpdateEncodedNote();
  if (!same_phrase) {
    UpdateExtents();
  }
  UpdateControls();
}

void PhraseWaveforms::UpdateControls() {
  const bool has_audio = !id_.empty() && !audio_.empty();
  for (const Track& track : tracks_) {
    const bool has_samples = track.pane->View()->HasSamples();
    track.play->setEnabled(track.source == PlaybackSource::kEncoded
                               ? encoded_shown_ && has_samples
                               : has_audio);
    track.reset->setEnabled(has_audio);
    for (QToolButton* button : {track.zoom_out, track.zoom_in, track.fit}) {
      button->setEnabled(has_samples);
    }
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
  switch (source) {
    case PlaybackSource::kOriginal:
      status = player_->Play(source, ToPcm16(audio_), play_start_);
      break;
    case PlaybackSource::kPreprocessed:
      status = player_->Play(source, ToPcm16(preprocessed_), play_start_);
      break;
    case PlaybackSource::kEncoded: {
      if (!encoded_shown_) {
        return;
      }
      // The same sound as the play start, or the beginning for a phrase
      // with no source audio.
      const int count = static_cast<int>(encoded_audio_.samples.size());
      const int start =
          audio_.empty()
              ? 0
              : std::clamp(play_start_ + encoded_audio_.source_offset, 0,
                           count);
      status = player_->Play(source, encoded_playback_, start);
      break;
    }
    case PlaybackSource::kBeforeOptimise:
    case PlaybackSource::kOptimised:
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
    for (const Track& track : tracks_) {
      track.pane->View()->SetMarker(sample);
    }
  }
  FollowPlayStart();
}

void PhraseWaveforms::FollowPlayStart() {
  const PlaybackState state = player_->State();
  if (state == PlaybackState::kStopped ||
      TrackFor(player_->Source()) == nullptr) {
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
  // The frames are copied into a non-const capture, so the lambdas move
  // them rather than copying
  runner_->Post([this, id = id_, audio = audio_, rendered_frames = frames,
                 synth = synth_] {
    auto result = std::make_shared<core::StatusOr<quality::EncodedAudio>>(
        quality::RenderEncodedAudio(*synth, audio, rendered_frames));
    QMetaObject::invokeMethod(
        this,
        [this, id, rendered_frames, result] {
          OnRendered(id, rendered_frames, std::move(*result));
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
      ViewOf(PhraseTab::kEncode)
          ->Clear(QStringLiteral("Cannot speak the frames: %1")
                      .arg(ToQString(result.GetStatus().Message())));
      UpdateControls();
    }
    emit RenderFinished();
    return;
  }
  Rendered& rendered = rendered_[id];
  rendered = {.frames = std::move(frames), .audio = std::move(*result)};
  if (current) {
    ShowEncoded(rendered);
    UpdateExtents();
    UpdateControls();
  }
  emit RenderFinished();
}

void PhraseWaveforms::ShowEncoded(const Rendered& rendered) {
  encoded_audio_ = rendered.audio;
  encoded_shown_ = true;
  // Shown and played at the original's level, so the two compare like
  // with like; the frames and exports are unchanged.
  // The pin is applied first: the DAC clips the codes, before any gain.
  encoded_clipped_ = core::synth::ClippedSamples(encoded_audio_.samples);
  std::vector<float> matched =
      ToFloat(core::synth::PinSamples(pin_, encoded_audio_.samples),
              encoded_audio_.level_gain);
  // The player applies the output filter itself.
  encoded_playback_ = ToPcm16(matched);
  matched = core::dsp::FilterSamples(filter_, matched);
  WaveformView* view = ViewOf(PhraseTab::kEncode);
  // Not followed: its own fit would be given to the others, which would
  // then no longer be fitted. UpdateExtents() fits them all together.
  following_ = true;
  view->SetSamples(std::move(matched), encoded_audio_.source_offset);
  following_ = false;
  UpdateEncodedNote();
  view->SetFrames(rendered.frames);
  view->SetFrameGridShown(grid_->isChecked());
}

void PhraseWaveforms::UpdateExtents() {
  // The union of the views with samples; a view without keeps its own.
  std::optional<TimeExtent> both;
  for (const Track& track : tracks_) {
    const WaveformView* view = track.pane->View();
    if (!view->HasSamples()) {
      continue;
    }
    const TimeExtent extent = view->ContentExtent();
    both = both.has_value()
               ? TimeExtent{.begin = std::min(both->begin, extent.begin),
                            .end = std::max(both->end, extent.end)}
               : extent;
  }
  following_ = true;
  for (const Track& track : tracks_) {
    WaveformView* view = track.pane->View();
    view->SetExtent(
        view->HasSamples() && both.has_value() ? *both : view->ContentExtent());
  }
  following_ = false;
  // New contents start fitted: the whole of them, as Fit shows them.
  WaveformView* lead = LeadView();
  lead->Fit();
  FollowAxis(lead);
  for (const Track& track : tracks_) {
    UpdateScrollBar(track);
  }
}

void PhraseWaveforms::FollowAxis(WaveformView* from) {
  if (following_ || !from->HasSamples()) {
    return;
  }
  following_ = true;
  for (const Track& track : tracks_) {
    WaveformView* to = track.pane->View();
    if (to == from || !to->HasSamples()) {
      continue;
    }
    // A tab not shown has its last size, so a fitted view's axis would
    // not fit it.
    if (from->IsFitted()) {
      to->Fit();
    } else {
      to->SetAxis(from->Axis());
    }
  }
  following_ = false;
}

WaveformView* PhraseWaveforms::LeadView() const {
  WaveformView* current = ViewOf(CurrentTab());
  if (current->HasSamples()) {
    return current;
  }
  for (const Track& track : tracks_) {
    if (track.pane->View()->HasSamples()) {
      return track.pane->View();
    }
  }
  return current;
}

void PhraseWaveforms::UpdateScrollBar(const Track& track) {
  // The scroll bar counts pixels from the extent's start.
  const WaveformView* view = track.pane->View();
  const TimeAxis& axis = view->Axis();
  const TimeExtent& extent = view->Extent();
  const int page = std::max(view->width(), 1);
  const auto total = static_cast<int>(
      std::lround((extent.end - extent.begin) / axis.samples_per_pixel));
  updating_scroll_bar_ = true;
  track.scroll_bar->setRange(0, std::max(total - page, 0));
  track.scroll_bar->setPageStep(page);
  track.scroll_bar->setValue(static_cast<int>(
      std::lround((axis.first - extent.begin) / axis.samples_per_pixel)));
  updating_scroll_bar_ = false;
}

void PhraseWaveforms::OnScrolled(const Track& track, int value) {
  if (updating_scroll_bar_) {
    return;
  }
  WaveformView* view = track.pane->View();
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
  if (pin_ == core::synth::OutputPin::kIo) {
    parts << QStringLiteral("I/O pin, 10-bit");
  } else if (encoded_shown_ && encoded_clipped_ > 0) {
    parts << (encoded_clipped_ == 1
                  ? QStringLiteral("1 sample clipped by the DAC")
                  : QStringLiteral("%1 samples clipped by the DAC")
                        .arg(encoded_clipped_));
  }
  if (filter_ != core::dsp::OutputFilter::kNone) {
    parts << QStringLiteral("%1 output filter")
                 .arg(ToQString(core::dsp::InfoFor(filter_).label));
  }
  if (!encoded_note_.isEmpty()) {
    parts << encoded_note_;
  }
  EncodedPane()->SetNote(parts.join(QStringLiteral(" · ")));
}

void PhraseWaveforms::SetPreprocessing(
    const core::preprocess::PreprocessOptions& options) {
  if (options == preprocessing_) {
    return;
  }
  preprocessing_ = options;
  UpdatePreprocessed();
  // The audio being played is no longer what the waveform shows.
  if (player_->State() != PlaybackState::kStopped &&
      player_->Source() == PlaybackSource::kPreprocessed) {
    player_->Stop();
  }
  RedrawPreprocessed();
}

void PhraseWaveforms::UpdatePreprocessed() {
  preprocessed_.clear();
  preprocessed_gain_ = 1.0;
  if (audio_.empty()) {
    return;
  }
  const std::vector<double> samples =
      core::preprocess::Preprocess(
          std::vector<double>(audio_.begin(), audio_.end()), preprocessing_)
          .samples;
  double peak = 0.0;
  for (const double v : samples) {
    peak = std::max(peak, std::abs(v));
  }
  if (peak > 1.0) {
    preprocessed_gain_ = 1.0 / peak;
  }
  preprocessed_.reserve(samples.size());
  for (const double v : samples) {
    preprocessed_.push_back(static_cast<float>(v * preprocessed_gain_));
  }
}

void PhraseWaveforms::RedrawPreprocessed() {
  if (!audio_.empty()) {
    // The same length as before, so the same extent and zoom.
    WaveformView* view = ViewOf(PhraseTab::kPreprocess);
    const TimeExtent extent = view->Extent();
    const TimeAxis axis = view->Axis();
    following_ = true;
    view->SetSamples(preprocessed_, 0);
    view->SetExtent(extent);
    view->SetAxis(axis);
    following_ = false;
  }
  UpdatePreprocessedLabels();
}

void PhraseWaveforms::SetOutputFilter(core::dsp::OutputFilter filter) {
  if (filter != filter_) {
    filter_ = filter;
    RedrawEncoded();
  }
}

void PhraseWaveforms::SetOutputPin(core::synth::OutputPin pin) {
  if (pin != pin_) {
    pin_ = pin;
    RedrawEncoded();
  }
}

void PhraseWaveforms::SetWaveformColours(WaveformColours choice) {
  for (const Track& track : tracks_) {
    track.pane->View()->SetWaveformColours(choice);
  }
}

void PhraseWaveforms::RedrawEncoded() {
  const auto found = rendered_.find(id_);
  if (encoded_shown_ && found != rendered_.end()) {
    // The same length as before, so the same extent and zoom.
    WaveformView* view = ViewOf(PhraseTab::kEncode);
    const TimeExtent extent = view->Extent();
    const TimeAxis axis = view->Axis();
    ShowEncoded(found->second);
    following_ = true;
    view->SetExtent(extent);
    view->SetAxis(axis);
    following_ = false;
  }
  UpdateEncodedNote();
}

void PhraseWaveforms::UpdatePreprocessedLabels() {
  WaveformPane* pane = PreprocessedPane();
  if (audio_.empty()) {
    pane->View()->SetShading({});
    pane->SetNote(QString());
    return;
  }
  QStringList notes;
  std::vector<TimeExtent> shading;
  if (preprocessing_.noise_gate_db.has_value()) {
    const std::vector<double> samples(audio_.begin(), audio_.end());
    for (const core::dsp::SampleSpan& span :
         core::preprocess::GatedSpans(samples, preprocessing_)) {
      shading.push_back({.begin = static_cast<double>(span.begin),
                         .end = static_cast<double>(span.end)});
    }
    const QString gate =
        QStringLiteral("noise gate %1 dB, %2 ms")
            .arg(QString::fromStdString(
                     core::FormatNumber(*preprocessing_.noise_gate_db)),
                 QString::fromStdString(
                     core::FormatNumber(preprocessing_.noise_gate_ms)));
    notes << (shading.empty()
                  ? QStringLiteral("%1: nothing is quiet enough to gate")
                        .arg(gate)
                  : QStringLiteral("%1: shaded stretches are turned down")
                        .arg(gate));
  }
  if (const QString after_gate = DescribeAfterGate(preprocessing_);
      !after_gate.isEmpty()) {
    notes << after_gate;
  }
  if (notes.isEmpty()) {
    notes << QStringLiteral("no preprocessing: the same as the original");
  }
  if (preprocessed_gain_ < 1.0) {
    notes << QStringLiteral("scaled by %1 dB to fit")
                 .arg(kDecibelsPerDecade * std::log10(preprocessed_gain_), 0,
                      'f', 1);
  }
  pane->View()->SetShading(std::move(shading));
  pane->SetNote(notes.join(QStringLiteral(" · ")));
}

QToolButton* PhraseWaveforms::PlayButton(PlaybackSource source) const {
  const Track* track = TrackFor(source);
  return track == nullptr ? nullptr : track->play;
}

bool PhraseWaveforms::ShowsPause(PlaybackSource source) const {
  const Track* track = TrackFor(source);
  return track != nullptr && track->shows_pause;
}

void PhraseWaveforms::SetPlaying(Track& track, bool playing) {
  track.shows_pause = playing;
  const QString name = AudioName(track.source);
  // The icon is drawn in the button text's colour, so it matches the
  // other controls in light and dark (plan §5, "Colours").
  track.play->setIcon(PlayPauseIcon(playing, *this));
  track.play->setToolTip(
      (playing ? QStringLiteral("Pause %1") : QStringLiteral("Play %1 (Space)"))
          .arg(name));
  track.play->setAccessibleName(
      (playing ? QStringLiteral("Pause %1") : QStringLiteral("Play %1"))
          .arg(name));
}

void PhraseWaveforms::changeEvent(QEvent* event) {
  if (event->type() == QEvent::PaletteChange) {
    for (Track& track : tracks_) {
      SetPlaying(track, track.shows_pause);
    }
  }
  QWidget::changeEvent(event);
}

void PhraseWaveforms::OnPlayerStateChanged() {
  const PlaybackState state = player_->State();
  const bool playing = state == PlaybackState::kPlaying;
  for (Track& track : tracks_) {
    SetPlaying(track, playing && player_->Source() == track.source);
    if (state == PlaybackState::kStopped) {
      track.pane->View()->SetPlayhead(std::nullopt);
    }
  }
}

void PhraseWaveforms::OnPlayerPositionChanged(int position) {
  if (player_->State() == PlaybackState::kStopped) {
    return;
  }
  // The Optimise window's audio has no playhead here.
  const Track* track = TrackFor(player_->Source());
  if (track == nullptr) {
    return;
  }
  WaveformView* view = track->pane->View();
  // The playhead in source samples, where the views count time.
  view->SetPlayhead(static_cast<double>(position - view->SourceOffset()));
}

}  // namespace speak5220::app
