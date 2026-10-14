/*
 * File:        audio_output.h
 * Module:      playback
 * Purpose:     Plays a buffer of 16-bit mono samples on the audio device
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PLAYBACK_AUDIO_OUTPUT_H_
#define SPEAK5220_GUI_PLAYBACK_AUDIO_OUTPUT_H_

#include <QByteArray>
#include <QString>
#include <cstdint>
#include <vector>

#include "core/util/status.h"

namespace speak5220::app {

// The audio device, as the player uses it: one buffer at a time, played
// from its start. GUI thread only.
class IAudioOutput {
 public:
  virtual ~IAudioOutput() = default;

  // The rate to play audio of `rate` at: `rate` itself if the device
  // accepts it, otherwise the device's own rate (the caller resamples).
  virtual int PlaybackRate(int rate) = 0;
  // Starts playing `samples` (mono, 16-bit) at `rate`, a rate
  // PlaybackRate() returned, replacing anything playing. Fails if the
  // device cannot be opened.
  virtual core::Status Start(std::vector<std::int16_t> samples, int rate) = 0;
  // Stops playing. Does nothing when not playing.
  virtual void Stop() = 0;
  // How much of the buffer the device has taken since Start(), in
  // microseconds.
  virtual std::int64_t ProcessedMicroseconds() = 0;
  // True once the whole buffer has been played, after Stop(), or after a
  // device error.
  virtual bool IsFinished() = 0;
  // Why the buffer stopped early: OK unless the device failed while
  // playing it (it was unplugged, or the sound server went away).
  virtual core::Status DeviceStatus() = 0;

  // Plays on the output device with `id` (AudioDeviceInfo::id), or on the
  // system's default for an empty id or one no device has now. Takes
  // effect from the next Start().
  virtual void SetDevice(const QByteArray& id) = 0;
};

// An output device, as Preferences lists it. A plain value type.
struct AudioDeviceInfo {
  // The system's id for the device (QAudioDevice::id()).
  QByteArray id;
  // Its name, as the system gives it.
  QString description;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PLAYBACK_AUDIO_OUTPUT_H_
