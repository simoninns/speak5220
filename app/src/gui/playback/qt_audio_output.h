/*
 * File:        qt_audio_output.h
 * Module:      playback
 * Purpose:     IAudioOutput over Qt Multimedia's QAudioSink
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_PLAYBACK_QT_AUDIO_OUTPUT_H_
#define SPEAK5220_GUI_PLAYBACK_QT_AUDIO_OUTPUT_H_

#include <QBuffer>
#include <QByteArray>
#include <cstdint>
#include <memory>
#include <vector>

#include "core/util/status.h"
#include "gui/playback/audio_output.h"

class QAudioSink;

namespace speak5220::app {

// Plan §2.4: IAudioOutput on the default output device through QAudioSink
// in pull mode, from a QBuffer, so the device's progress is known. Mono
// 16-bit at the rate asked for if the device takes it; otherwise its
// preferred rate, and its preferred channel count with the samples
// copied to each channel. A thin adapter: checked by hand, not unit
// tested (TESTING.md). GUI thread only.
class QtAudioOutput : public IAudioOutput {
 public:
  QtAudioOutput();
  ~QtAudioOutput() override;
  QtAudioOutput(const QtAudioOutput&) = delete;
  QtAudioOutput& operator=(const QtAudioOutput&) = delete;

  int PlaybackRate(int rate) override;
  core::Status Start(std::vector<std::int16_t> samples, int rate) override;
  void Stop() override;
  std::int64_t ProcessedMicroseconds() override;
  bool IsFinished() override;

 private:
  // Stop(), callable from the destructor.
  void Close();

  std::unique_ptr<QAudioSink> sink_;
  QByteArray bytes_;
  QBuffer buffer_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_PLAYBACK_QT_AUDIO_OUTPUT_H_
