/*
 * File:        player.h
 * Module:      playback
 * Purpose:     Plays a phrase's original or encoded audio from the
 *              play-start marker and tracks the playhead
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PLAYBACK_PLAYER_H_
#define SPEAK5220_GUI_PLAYBACK_PLAYER_H_

#include <QObject>
#include <QTimer>
#include <cstdint>
#include <span>
#include <vector>

#include "core/dsp/resampler.h"
#include "core/util/clock.h"
#include "core/util/status.h"
#include "gui/playback/audio_output.h"

namespace speak5220::app {

// `samples` (full scale +-1) as 16-bit samples, rounded and clipped.
// Pure.
std::vector<std::int16_t> ToPcm16(std::span<const float> samples);

// What is playing: a waveform pane's audio, or in the Optimise window a
// phrase's encoding before Optimise or the optimised one.
enum class PlaybackSource { kOriginal, kEncoded, kBeforeOptimise, kOptimised };

enum class PlaybackState { kStopped, kPlaying, kPaused };

// Plan §5 and Phase 6: plays one buffer of 8 kHz samples at a time, so
// starting one pane stops the other. Play starts at a sample (the
// play-start marker), Pause holds the position and Resume goes on from
// it; at the end, or on Stop, the position returns to where play started.
//
// The playhead is the time since the device started, but never more than
// the device has taken (IAudioOutput::ProcessedMicroseconds()), so it
// neither runs ahead of the audio nor jumps with the device's buffer.
// It moves on a timer while playing (kTickIntervalMs), or when Tick() is
// called. When the device does not take 8 kHz, the samples are resampled
// to its rate. GUI thread only; the output, clock and resampler must
// outlive the player.
class Player : public QObject {
  Q_OBJECT

 public:
  // The rate of the samples played (plan §3.1).
  static constexpr int kSampleRate = 8000;
  // How often the playhead moves while playing: about 30 Hz.
  static constexpr int kTickIntervalMs = 33;

  Player(IAudioOutput& output, core::IClock& clock,
         core::dsp::IResampler& resampler, QObject* parent = nullptr);
  ~Player() override;
  Player(const Player&) = delete;
  Player& operator=(const Player&) = delete;

  // Plays `samples` from `start`, for `source`, stopping anything playing.
  // A start at or past the end plays from the beginning. Fails, leaving
  // the player stopped, for no samples or if the device cannot play.
  core::Status Play(PlaybackSource source, std::vector<std::int16_t> samples,
                    int start);
  // Pauses at the playhead. Does nothing unless playing.
  void Pause();
  // Goes on from where Pause() stopped. Does nothing unless paused.
  core::Status Resume();
  // Stops; the playhead returns to where play started.
  void Stop();

  PlaybackState State() const { return state_; }
  // The source of what is playing or paused; meaningless when stopped.
  PlaybackSource Source() const { return source_; }
  // The playhead, in the samples given to Play().
  int Position() const { return position_; }

  // Moves the playhead to where the audio has got to, and stops at the
  // end. Called by the timer while playing.
  void Tick();

 signals:
  void StateChanged();
  void PositionChanged(int position);

 private:
  core::Status StartOutput(int from);
  void SetState(PlaybackState state);

  IAudioOutput* output_;
  core::IClock* clock_;
  core::dsp::IResampler* resampler_;
  QTimer timer_;

  PlaybackState state_ = PlaybackState::kStopped;
  PlaybackSource source_ = PlaybackSource::kOriginal;
  std::vector<std::int16_t> samples_;
  // Where play started, where the device's buffer starts, the playhead.
  int start_ = 0;
  int base_ = 0;
  int position_ = 0;
  // The clock when the device started.
  std::int64_t started_us_ = 0;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PLAYBACK_PLAYER_H_
