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

std::vector<AudioDeviceInfo> QtAudioOutput::Devices() {
  std::vector<AudioDeviceInfo> devices;
  const QAudioDevice preferred = QMediaDevices::defaultAudioOutput();
  if (!preferred.isNull()) {
    devices.push_back(
        {.id = preferred.id(), .description = preferred.description()});
  }
  for (const QAudioDevice& device : QMediaDevices::audioOutputs()) {
    if (device.id() != preferred.id()) {
      devices.push_back(
          {.id = device.id(), .description = device.description()});
    }
  }
  return devices;
}

void QtAudioOutput::SetDevice(const QByteArray& id) {
  device_id_ = id;
  if (!id.isEmpty() && Device().id() != id) {
    core::Logger(core::kLogModuleGui)
        ->warn(
            "the chosen audio device is not there; playing on the "
            "system's default");
  }
}

QAudioDevice QtAudioOutput::Device() const {
  if (!device_id_.isEmpty()) {
    for (const QAudioDevice& device : QMediaDevices::audioOutputs()) {
      if (device.id() == device_id_) {
        return device;
      }
    }
  }
  return QMediaDevices::defaultAudioOutput();
}

int QtAudioOutput::PlaybackRate(int rate) {
  const QAudioDevice device = Device();
  if (device.isNull() || device.isFormatSupported(MonoFormat(rate))) {
    return rate;
  }
  const int preferred = device.preferredFormat().sampleRate();
  return preferred > 0 ? preferred : rate;
}

core::Status QtAudioOutput::Start(std::vector<std::int16_t> samples, int rate) {
  Stop();
  const QAudioDevice device = Device();
  device_name_ = device.description();
  if (device.isNull()) {
    return core::UnavailableError(
        "there is no audio output device: connect one, or check the "
        "system's sound settings");
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
    return core::UnavailableError(
        "cannot play on " + device.description().toStdString() +
        ": it may be in use or disconnected; choose another in "
        "Preferences");
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

core::Status QtAudioOutput::DeviceStatus() {
  if (sink_ == nullptr) {
    return core::OkStatus();
  }
  // Only these are failures. Before Qt 6.11 the sink also reported an
  // underrun when a pull-mode buffer ran out, which is the normal end.
  const QAudio::Error error = sink_->error();
  if (error == QAudio::OpenError) {
    return core::UnavailableError("cannot open the audio device " +
                                  device_name_.toStdString());
  }
  if (error != QAudio::IOError && error != QAudio::FatalError) {
    return core::OkStatus();
  }
  return core::UnavailableError(
      "the audio device " + device_name_.toStdString() +
      " stopped while playing: check that it is still connected, or "
      "choose another in Preferences");
}

}  // namespace speak5220::app
