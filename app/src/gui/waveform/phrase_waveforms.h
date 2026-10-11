/*
 * File:        phrase_waveforms.h
 * Module:      waveform
 * Purpose:     The phrase editor's two waveform panes, original and
 *              encoded: rendering, linked time axes, the play-start marker
 *              and playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_PHRASE_WAVEFORMS_H_
#define SPEAK5220_GUI_WAVEFORM_PHRASE_WAVEFORMS_H_

#include <QString>
#include <QWidget>
#include <cstdint>
#include <map>
#include <optional>
#include <vector>

#include "core/dsp/output_filter.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/quality/encoded_audio.h"
#include "core/synth/output_pin.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gui/playback/player.h"
#include "gui/undo/project_edits.h"

class QCheckBox;
class QEvent;
class QScrollBar;
class QToolButton;

namespace speak5220::app {

class WaveformPane;
class WaveformView;

// Plan §5 and Phase 6: the original audio above the encoded audio, the
// same height, with one scroll bar and one row of controls under both:
// play or pause each (labelled Original and Encoded), reset the play
// start, Frame grid, zoom out, zoom in and fit.
//
// - The encoded audio is the cached frames spoken through the emulator
//   and lined up with the original (RenderEncodedAudio()), in the
//   background on the ITaskRunner; the result comes back queued and is
//   kept per phrase until its frames change.
// - Both panes always have the same time axis, over the union of their
//   extents, so the same sound is at the same place: zooming or
//   scrolling either one, or with the shared controls, moves both.
// - A click in either pane moves the phrase's play start there, and the
//   reset button moves it to 0, each one undoable step
//   (ProjectEdits::SetPlayStart()). The original plays from the play
//   start, and the encoded audio from the same sound; moving the play
//   start while a pane plays or is paused takes playback there too.
// - The encoded audio is shown and played at the original's level
//   (EncodedAudio::level_gain), which the encoded pane's note gives in
//   dB: the chip's output is about 15 dB quieter than a normalised
//   recording, which would make the two hard to compare.
// - With a squelch (SetSquelch()), the stretches it mutes are shaded on
//   the original audio (encoder::SquelchedSpans()), and the original
//   pane's note gives its level, so what will be left out of the encoding
//   and of the quality measure can be seen before encoding.
// - With an output filter (SetOutputFilter()), the encoded audio is shown
//   through it (core::dsp::FilterSamples()), as that computer's audio
//   stage would deliver it, and the encoded pane's note names it; the
//   Player plays it through the same filter. The original is unchanged.
// - The encoded audio is taken from the chip's SPEAKER pin, whose DAC
//   clips each code to 8 bits, or from its I/O pin's 10-bit codes
//   (SetOutputPin()): shown and played so, and the encoded pane's note
//   says how many samples the DAC clipped, or that it is the I/O pin. The
//   level match is measured on the codes, so it is the same for both.
// - Each pane has its own play button, which shows pause while that pane
//   plays (Space in a pane's view does the same); one pane plays at a
//   time, through the Player, and its view shows the playhead.
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

  // Shows `phrase`, or clears both panes when null (no phrase, or
  // several, selected). Showing the phrase already shown keeps the zoom,
  // and renders again only if its frames changed. `encoded_note` is shown
  // beside the encoded pane's title (such as "stale").
  void ShowPhrase(const core::project::Phrase* phrase,
                  const QString& encoded_note = QString());

  // The squelch of the phrase shown, in dB, or none: as the encoder
  // panel's settings have it, changes not yet applied included.
  void SetSquelch(std::optional<double> squelch_db);
  std::optional<double> Squelch() const { return squelch_db_; }

  // The audio stage the encoded audio is shown through; none at first.
  // Changing it redraws the encoded audio, keeping the zoom.
  void SetOutputFilter(core::dsp::OutputFilter filter);
  core::dsp::OutputFilter Filter() const { return filter_; }
  // The chip's pin the encoded audio is taken from; SPEAKER at first.
  // Changing it redraws the encoded audio, keeping the zoom.
  void SetOutputPin(core::synth::OutputPin pin);
  core::synth::OutputPin Pin() const { return pin_; }

  WaveformPane* OriginalPane() const { return original_; }
  WaveformPane* EncodedPane() const { return encoded_; }
  QScrollBar* ScrollBar() const { return scroll_bar_; }
  // The play/pause button of a pane.
  QToolButton* PlayButton(PlaybackSource source) const;
  // True while `source`'s button shows pause.
  bool ShowsPause(PlaybackSource source) const;
  QToolButton* ResetButton() const { return reset_; }
  QToolButton* ZoomInButton() const { return zoom_in_; }
  QToolButton* ZoomOutButton() const { return zoom_out_; }
  QToolButton* FitButton() const { return fit_; }
  QCheckBox* GridBox() const { return grid_; }
  // Renders started and not yet finished.
  int RendersRunning() const { return renders_running_; }

  // Plays the pane's audio from the play start, or pauses or resumes it
  // if that pane is already playing or paused.
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

 protected:
  // Redraws the play/pause icons in the new palette's colours.
  void changeEvent(QEvent* event) override;

 private:
  struct Rendered {
    std::vector<core::lpc::Frame> frames;
    core::quality::EncodedAudio audio;
  };

  void Render(const std::vector<core::lpc::Frame>& frames);
  void OnRendered(const core::project::PhraseId& id,
                  std::vector<core::lpc::Frame> frames,
                  core::StatusOr<core::quality::EncodedAudio> result);
  void ShowEncoded(const Rendered& rendered);
  // Shows the encoded audio again, for a new filter or pin, keeping the
  // zoom.
  void RedrawEncoded();
  // The encoded pane's note: the level match, then the given note.
  void UpdateEncodedNote();
  // The shading of the squelched stretches and the original pane's note.
  void UpdateShading();
  // Sets both views' extents for new contents (the union of the two) and
  // fits both to it.
  void UpdateExtents();
  void FollowAxis(WaveformView* from, WaveformView* to);
  // The view the shared controls act on: the original, or the encoded
  // audio when there is no original.
  WaveformView* LeadView() const;
  void UpdateScrollBar();
  void OnScrolled(int value);
  void SetPlaying(PlaybackSource source, bool playing);
  // Playing audio starts again from the play start; paused audio stops.
  void FollowPlayStart();
  void OnPlayerStateChanged();
  void OnPlayerPositionChanged(int position);
  WaveformView* PlayingView() const;

  ProjectEdits* edits_;
  Player* player_;
  core::synth::ISynth* synth_;
  core::ITaskRunner* runner_;
  WaveformPane* original_;
  WaveformPane* encoded_;
  QScrollBar* scroll_bar_;
  QToolButton* play_original_;
  QToolButton* play_encoded_;
  QToolButton* reset_;
  QCheckBox* grid_;
  QToolButton* zoom_out_;
  QToolButton* zoom_in_;
  QToolButton* fit_;

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
  std::optional<double> squelch_db_;
  core::dsp::OutputFilter filter_ = core::dsp::OutputFilter::kNone;
  core::synth::OutputPin pin_ = core::synth::OutputPin::kSpeaker;
  bool original_shows_pause_ = false;
  bool encoded_shows_pause_ = false;
  // The latest render of each phrase.
  std::map<core::project::PhraseId, Rendered> rendered_;
  int renders_running_ = 0;
  // Set while one view's axis is copied to the other, and while the
  // scroll bar is being moved to match the axis.
  bool following_ = false;
  bool updating_scroll_bar_ = false;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_PHRASE_WAVEFORMS_H_
