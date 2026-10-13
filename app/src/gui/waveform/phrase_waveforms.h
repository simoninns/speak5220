/*
 * File:        phrase_waveforms.h
 * Module:      waveform
 * Purpose:     The phrase editor's tabs, each a waveform with its own
 *              controls: the original, the preprocessed and the encoded
 *              audio; rendering, linked time axes, the play-start marker
 *              and playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_PHRASE_WAVEFORMS_H_
#define SPEAK5220_GUI_WAVEFORM_PHRASE_WAVEFORMS_H_

#include <QPixmap>
#include <QString>
#include <QWidget>
#include <array>
#include <cstdint>
#include <map>
#include <vector>

#include "core/dsp/output_filter.h"
#include "core/lpc/frame.h"
#include "core/preprocess/preprocess.h"
#include "core/project/phrase.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/playback/player.h"
#include "gui/undo/project_edits.h"
#include "gui/waveform/phrase_tab.h"

class QCheckBox;
class QEvent;
class QScrollBar;
class QScrollArea;
class QSplitter;
class QStackedWidget;
class QTabBar;
class QToolButton;

namespace speak5220::app {

class WaveformPane;
class WaveformView;

// Plan §5, Phase 6 and Phase 11: the phrase editor's three tabs (PhraseTab).
// Under the tab bar are two areas with one splitter between them, which
// the tabs share, so neither area moves or changes size when another tab
// is chosen and the waveforms can be compared by switching tabs:
//
// - The waveform area: the tab's waveform with its own scroll bar and
//   row of controls under it (play or pause, reset the play start, zoom
//   out, zoom in and fit; Frame grid on the Encode tab).
// - The settings area: the tab's settings (SetControls()) in a scroll
//   area of their own, so however many there are, the area keeps its size
//   and scrolls instead.
//
// The tabs:
//
// - Original: the recording as imported.
// - Preprocess: the recording preprocessed (SetPreprocessing(),
//   preprocess::Preprocess()): what the encoder aims at and the quality is
//   measured against. Where the preprocessing would take it past full
//   scale, it is shown and played scaled down to fit, and the note says
//   so. With a noise gate, the stretches it turns down by more than 6 dB
//   are shaded (preprocess::GatedSpans()) and the note gives its level and
//   speed, so what will be muted in the encoding and the quality measure
//   can be seen before encoding.
// - Encode: the cached frames spoken through the emulator and lined up
//   with the original (RenderEncodedAudio()), in the background on the
//   ITaskRunner; the result comes back queued and is kept per phrase
//   until its frames change.
//
// - The three waveforms always have the same time axis, over the union of
//   their extents, so the same sound is at the same place in every tab:
//   zooming or scrolling one, with its controls or in its view, moves the
//   others. A tab not shown keeps its last size, so a view that shows its
//   whole extent makes the others fit theirs rather than copying its axis.
// - The play start is the phrase's, shared by the tabs. A click in any
//   waveform moves it there, and a tab's reset button moves it to 0, each
//   one undoable step (ProjectEdits::SetPlayStart()). The recordings play
//   from the play start, and the encoded audio from the same sound; moving
//   the play start while a waveform plays or is paused takes playback
//   there too.
// - The encoded audio is shown and played at the original's level
//   (EncodedAudio::level_gain), which its note gives in dB: the chip's
//   output is about 15 dB quieter than a normalised recording, which would
//   make the two hard to compare.
// - With an output filter (SetOutputFilter()), the encoded audio is shown
//   through it (core::dsp::FilterSamples()), as that computer's audio
//   stage would deliver it, and its note names it; the Player plays it
//   through the same filter. The recordings are unchanged.
// - The encoded audio is taken from the chip's SPEAKER pin, whose DAC
//   clips each code to 8 bits, or from its I/O pin's 10-bit codes
//   (SetOutputPin()): shown and played so, and its note says how many
//   samples the DAC clipped, or that it is the I/O pin. The level match is
//   measured on the codes, so it is the same for both.
// - Each tab's play button shows pause while its waveform plays (Space in
//   the view does the same); one waveform plays at a time, through the
//   Player, and its view shows the playhead.
//
// GUI thread only. The edits, player, synth and runner must outlive the
// widget; the destructor waits for the runner.
class PhraseWaveforms : public QWidget {
  Q_OBJECT

 public:
  PhraseWaveforms(ProjectEdits& edits, Player& player,
                  core::synth::ISynth& synth, core::ITaskRunner& runner,
                  QWidget* parent = nullptr);
  ~PhraseWaveforms() override;
  PhraseWaveforms(const PhraseWaveforms&) = delete;
  PhraseWaveforms& operator=(const PhraseWaveforms&) = delete;

  // Shows `phrase`, or clears every waveform when null (no phrase, or
  // several, selected), showing the logo faintly in their place. Showing
  // the phrase already shown keeps the zoom, and renders again only if its
  // frames changed. `encoded_note` is shown beside the encoded waveform's
  // title (such as "stale").
  void ShowPhrase(const core::project::Phrase* phrase,
                  const QString& encoded_note = QString());

  // The preprocessing of the phrase shown: as the encoder panel's
  // settings have it, changes not yet applied included. None at first.
  // Changing it redraws the preprocessed recording, keeping the zoom, and
  // stops it if it is playing.
  void SetPreprocessing(const core::preprocess::PreprocessOptions& options);
  const core::preprocess::PreprocessOptions& Preprocessing() const {
    return preprocessing_;
  }

  // The audio stage the encoded audio is shown through; none at first.
  // Changing it redraws the encoded audio, keeping the zoom.
  void SetOutputFilter(core::dsp::OutputFilter filter);
  core::dsp::OutputFilter Filter() const { return filter_; }
  // The chip's pin the encoded audio is taken from; SPEAKER at first.
  // Changing it redraws the encoded audio, keeping the zoom.
  void SetOutputPin(core::synth::OutputPin pin);
  core::synth::OutputPin Pin() const { return pin_; }

  // Puts `controls` in `tab`'s settings area, which scrolls when they do
  // not fit, and takes ownership. A tab's area says it has no settings
  // until given some.
  void SetControls(PhraseTab tab, QWidget* controls);
  // The tab shown; Original at first. Choosing one, here or by its tab,
  // signals CurrentTabChanged().
  void SetCurrentTab(PhraseTab tab);
  PhraseTab CurrentTab() const;
  QTabBar* Tabs() const { return tab_bar_; }

  WaveformPane* Pane(PhraseTab tab) const { return TrackOf(tab).pane; }
  WaveformPane* OriginalPane() const { return Pane(PhraseTab::kOriginal); }
  WaveformPane* PreprocessedPane() const {
    return Pane(PhraseTab::kPreprocess);
  }
  WaveformPane* EncodedPane() const { return Pane(PhraseTab::kEncode); }
  // The splitter between the waveform area and the settings area, shared
  // by every tab.
  QSplitter* Splitter() const { return splitter_; }
  // The scroll area holding a tab's settings.
  QScrollArea* SettingsArea(PhraseTab tab) const {
    return TrackOf(tab).settings;
  }
  QScrollBar* ScrollBar(PhraseTab tab) const { return TrackOf(tab).scroll_bar; }
  // The play/pause button of the waveform that plays `source`; nullptr
  // for the Optimise window's sources.
  QToolButton* PlayButton(PlaybackSource source) const;
  // True while `source`'s button shows pause.
  bool ShowsPause(PlaybackSource source) const;
  QToolButton* ResetButton(PhraseTab tab) const { return TrackOf(tab).reset; }
  QToolButton* ZoomInButton(PhraseTab tab) const {
    return TrackOf(tab).zoom_in;
  }
  QToolButton* ZoomOutButton(PhraseTab tab) const {
    return TrackOf(tab).zoom_out;
  }
  QToolButton* FitButton(PhraseTab tab) const { return TrackOf(tab).fit; }
  // The Encode tab's Frame grid.
  QCheckBox* GridBox() const { return grid_; }
  // Renders started and not yet finished.
  int RendersRunning() const { return renders_running_; }

  // Plays the audio of `source` (the original, the preprocessed or the
  // encoded audio) from the play start, or pauses or resumes it if it is
  // already playing or paused.
  void PlayPause(PlaybackSource source);
  // Moves the play start to `source_sample`, rounded and limited to the
  // audio, as one undoable step. Playback follows it: audio playing
  // starts again from the new play start, and audio paused stops, so the
  // next play starts there. It does so even when the play start is
  // already there.
  void MovePlayStart(double source_sample);

 signals:
  // A render finished, successfully or not.
  void RenderFinished();
  // Moving the play start or playing failed.
  void Failed(const QString& title, const QString& message);
  // Another tab is shown.
  void CurrentTabChanged(PhraseTab tab);

 protected:
  // Redraws the play/pause icons in the new palette's colours.
  void changeEvent(QEvent* event) override;

 private:
  // A tab's waveform and its controls.
  struct Track {
    PhraseTab tab = PhraseTab::kOriginal;
    PlaybackSource source = PlaybackSource::kOriginal;
    QScrollArea* settings = nullptr;
    WaveformPane* pane = nullptr;
    QScrollBar* scroll_bar = nullptr;
    QToolButton* play = nullptr;
    QToolButton* reset = nullptr;
    QToolButton* zoom_out = nullptr;
    QToolButton* zoom_in = nullptr;
    QToolButton* fit = nullptr;
    bool shows_pause = false;
  };

  struct Rendered {
    std::vector<core::lpc::Frame> frames;
    core::quality::EncodedAudio audio;
  };

  // Makes the tab's waveform, its controls and its settings area.
  void BuildTrack(Track& track, const QString& tab_title,
                  const QString& pane_title, const QString& audio_name);
  const Track& TrackOf(PhraseTab tab) const;
  Track& TrackOf(PhraseTab tab);
  // The track that plays `source`, or nullptr.
  const Track* TrackFor(PlaybackSource source) const;
  WaveformView* ViewOf(PhraseTab tab) const;

  void Render(const std::vector<core::lpc::Frame>& frames);
  void OnRendered(const core::project::PhraseId& id,
                  std::vector<core::lpc::Frame> frames,
                  core::StatusOr<core::quality::EncodedAudio> result);
  void ShowEncoded(const Rendered& rendered);
  // Shows the encoded audio again, for a new filter or pin, keeping the
  // zoom.
  void RedrawEncoded();
  // The encoded waveform's note: the level match, then the given note.
  void UpdateEncodedNote();
  // Makes the preprocessed recording of audio_ again.
  void UpdatePreprocessed();
  // Shows the preprocessed recording again, keeping the zoom, with its
  // shading and note.
  void RedrawPreprocessed();
  // The preprocessed waveform's shading and note.
  void UpdatePreprocessedLabels();
  // Enables the controls that have something to act on.
  void UpdateControls();
  // Sets every view's extent for new contents (the union of those with
  // samples) and fits them all to it.
  void UpdateExtents();
  // Gives the other views the axis of `from`, or fits them if it is
  // fitted.
  void FollowAxis(WaveformView* from);
  // The view whose axis the others follow when contents change: the
  // current tab's, or the first with samples when it has none.
  WaveformView* LeadView() const;
  void UpdateScrollBar(const Track& track);
  void OnScrolled(const Track& track, int value);
  void SetPlaying(Track& track, bool playing);
  // Playing audio starts again from the play start; paused audio stops.
  void FollowPlayStart();
  void OnPlayerStateChanged();
  void OnPlayerPositionChanged(int position);

  ProjectEdits* edits_;
  Player* player_;
  core::synth::ISynth* synth_;
  core::ITaskRunner* runner_;
  QTabBar* tab_bar_;
  QSplitter* splitter_;
  // The tabs' waveforms and settings, in PhraseTab order.
  QStackedWidget* waveform_stack_;
  QStackedWidget* settings_stack_;
  // In PhraseTab order.
  std::array<Track, 3> tracks_;
  QCheckBox* grid_ = nullptr;
  // The logo the waveforms show faintly with no phrase; null where the
  // executable's resources are not linked in (the tests).
  QPixmap logo_;

  // The phrase shown: its id, audio, play start and frames.
  core::project::PhraseId id_;
  std::vector<float> audio_;
  int play_start_ = 0;
  std::vector<core::lpc::Frame> frames_;
  // The encoded audio of frames_, once rendered.
  bool encoded_shown_ = false;
  core::quality::EncodedAudio encoded_audio_;
  // The encoded audio at the original's level, as played.
  std::vector<std::int16_t> encoded_playback_;
  // The samples of encoded_audio_ the DAC clips.
  int encoded_clipped_ = 0;
  // The note given with the phrase (such as "stale").
  QString encoded_note_;
  core::preprocess::PreprocessOptions preprocessing_;
  // audio_ preprocessed, scaled by preprocessed_gain_ (at most 1) so it
  // stays within full scale.
  std::vector<float> preprocessed_;
  double preprocessed_gain_ = 1.0;
  core::dsp::OutputFilter filter_ = core::dsp::OutputFilter::kNone;
  core::synth::OutputPin pin_ = core::synth::OutputPin::kSpeaker;
  // The latest render of each phrase.
  std::map<core::project::PhraseId, Rendered> rendered_;
  int renders_running_ = 0;
  // Set while one view's axis is given to the others, and while a scroll
  // bar is being moved to match its view.
  bool following_ = false;
  bool updating_scroll_bar_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_PHRASE_WAVEFORMS_H_
