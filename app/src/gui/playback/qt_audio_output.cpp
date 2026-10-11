/*
 * File:        qt_audio_output.cpp
 * Module:      playback
 * Purpose:     IAudioOutput over Qt Multimedia's QAudioSink
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/playback/qt_audio_output.h"

#include <QAudio>
#include <QAudioDevice>
#include <QAudioFormat>
#include <QAudioSink>
#include <QBuffer>
#include <QByteArray>
#include <QIODevice>
#include <QMediaDevices>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <memory>
#include <span>
#include <string>
#include <vector>

#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::app {

namespace {

QAudioFormat MonoFormat(int rate) {
  QAudioFormat format;
  format.setSampleRate(rate);
  format.setChannelCount(1);
  format.setSampleFormat(QAudioFormat::Int16);
  return format;
}

}  // namespace

QtAudioOutput::QtAudioOutput() = default;

QtAudioOutput::~QtAudioOutput() { Close(); }

int QtAudioOutput::PlaybackRate(int rate) {
  const QAudioDevice device = QMediaDevices::defaultAudioOutput();
  if (device.isNull() || device.isFormatSupported(MonoFormat(rate))) {
    return rate;
  }
  const int preferred = device.preferredFormat().sampleRate();
  return preferred > 0 ? preferred : rate;
}

core::Status QtAudioOutput::Start(std::vector<std::int16_t> samples, int rate) {
  Stop();
  const QAudioDevice device = QMediaDevices::defaultAudioOutput();
  if (device.isNull()) {
    return core::UnavailableError("there is no audio output device");
  }
  QAudioFormat format = MonoFormat(rate);
  if (!device.isFormatSupported(format)) {
    // Some devices take only their own channel count: each sample goes to
    // every channel.
    format.setChannelCount(
        std::max(1, device.preferredFormat().channelCount()));
  }
  const auto channels = static_cast<std::size_t>(format.channelCount());
  bytes_.resize(static_cast<qsizetype>(samples.size()) *
                static_cast<qsizetype>(channels) *
                static_cast<qsizetype>(sizeof(std::int16_t)));
  const std::span<char> out(bytes_.data(),
                            static_cast<std::size_t>(bytes_.size()));
  std::size_t at = 0;
  for (const std::int16_t sample : samples) {
    for (std::size_t c = 0; c < channels; ++c) {
      std::memcpy(&out[at], &sample, sizeof(sample));
      at += sizeof(sample);
    }
  }
  buffer_.setBuffer(&bytes_);
  buffer_.open(QIODevice::ReadOnly);
  sink_ = std::make_unique<QAudioSink>(device, format);
  sink_->start(&buffer_);
  if (sink_->error() != QAudio::NoError) {
    Stop();
    return core::UnavailableError("cannot play on " +
                                  device.description().toStdString());
  }
  core::Logger(core::kLogModuleGui)
      ->debug("playing {} samples at {} Hz, {} channel(s), on {}",
              samples.size(), rate, channels,
              device.description().toStdString());
  return core::OkStatus();
}

void QtAudioOutput::Stop() { Close(); }

void QtAudioOutput::Close() {
  if (sink_ != nullptr) {
    sink_->stop();
    sink_.reset();
  }
  buffer_.close();
}

std::int64_t QtAudioOutput::ProcessedMicroseconds() {
  return sink_ != nullptr ? sink_->processedUSecs() : 0;
}

bool QtAudioOutput::IsFinished() {
  if (sink_ == nullptr || sink_->error() != QAudio::NoError) {
    return true;
  }
  const QAudio::State state = sink_->state();
  return state == QAudio::IdleState || state == QAudio::StoppedState;
}

}  // namespace speak5220::app
