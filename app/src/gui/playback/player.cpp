/*
 * File:        player.cpp
 * Module:      playback
 * Purpose:     Plays a phrase's original or encoded audio from the
 *              play-start marker and tracks the playhead
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/playback/player.h"

#include <QByteArray>
#include <QObject>
#include <QString>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/output_filter.h"
#include "core/dsp/resampler.h"
#include "core/synth/chip_clock.h"
#include "core/util/clock.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "gui/playback/audio_output.h"

namespace speak5220::app {

namespace {

constexpr std::int64_t kMicrosecondsPerSecond = 1000000;
// Full scale of a 16-bit sample.
constexpr float kFullScale = 32768.0F;

// `samples` as fractions of full scale.
std::vector<float> ToFloat(std::span<const std::int16_t> samples) {
  std::vector<float> out;
  out.reserve(samples.size());
  for (const std::int16_t v : samples) {
    out.push_back(static_cast<float>(v) / kFullScale);
  }
  return out;
}

}  // namespace

std::vector<std::int16_t> ToPcm16(std::span<const float> samples) {
  std::vector<std::int16_t> out;
  out.reserve(samples.size());
  for (const float v : samples) {
    const float scaled = std::round(v * kFullScale);
    out.push_back(static_cast<std::int16_t>(std::clamp(
        scaled, static_cast<float>(std::numeric_limits<std::int16_t>::min()),
        static_cast<float>(std::numeric_limits<std::int16_t>::max()))));
  }
  return out;
}

bool IsChipOutput(PlaybackSource source) {
  switch (source) {
    case PlaybackSource::kOriginal:
    case PlaybackSource::kPreprocessed:
      return false;
    case PlaybackSource::kEncoded:
    case PlaybackSource::kBeforeOptimise:
    case PlaybackSource::kOptimised:
      return true;
  }
  return true;
}

Player::Player(IAudioOutput& output, core::IClock& clock,
               core::dsp::IResampler& resampler, QObject* parent)
    : QObject(parent),
      output_(&output),
      clock_(&clock),
      resampler_(&resampler) {
  timer_.setInterval(kTickIntervalMs);
  connect(&timer_, &QTimer::timeout, this, &Player::Tick);
}

Player::~Player() {
  if (state_ == PlaybackState::kPlaying) {
    output_->Stop();
  }
}

core::Status Player::Play(PlaybackSource source,
                          std::vector<std::int16_t> samples, int start) {
  Stop();
  if (samples.empty()) {
    return core::FailedPreconditionError("there is no audio to play");
  }
  samples_ = std::move(samples);
  source_ = source;
  filtered_.clear();
  if (IsFiltered(source)) {
    // The whole buffer, so the filters' state at any start is as it
    // would be had it played from the beginning.
    filtered_ = core::dsp::FilterDacOutput(filter_, ToFloat(samples_),
                                           SampleRateOf(source));
  }
  start_ =
      (start < 0 || std::cmp_greater_equal(start, samples_.size())) ? 0 : start;
  position_ = start_;
  return StartOutput(start_);
}

void Player::Pause() {
  if (state_ != PlaybackState::kPlaying) {
    return;
  }
  Tick();
  if (state_ != PlaybackState::kPlaying) {
    // It had reached the end.
    return;
  }
  output_->Stop();
  SetState(PlaybackState::kPaused);
}

core::Status Player::Resume() {
  if (state_ != PlaybackState::kPaused) {
    return core::OkStatus();
  }
  return StartOutput(position_);
}

void Player::Stop() {
  if (state_ == PlaybackState::kStopped) {
    return;
  }
  if (state_ == PlaybackState::kPlaying) {
    output_->Stop();
  }
  position_ = start_;
  SetState(PlaybackState::kStopped);
  emit PositionChanged(position_);
}

void Player::Tick() {
  if (state_ != PlaybackState::kPlaying) {
    return;
  }
  const std::int64_t elapsed = std::min(clock_->NowMicroseconds() - started_us_,
                                        output_->ProcessedMicroseconds());
  const std::int64_t played =
      elapsed * SampleRateOf(source_) / kMicrosecondsPerSecond;
  const auto end = static_cast<std::int64_t>(samples_.size());
  if (output_->IsFinished() || base_ + played >= end) {
    const core::Status device = output_->DeviceStatus();
    Stop();
    if (!device.IsOk()) {
      core::Logger(core::kLogModuleGui)
          ->warn("playback stopped: {}", device.Message());
      emit DeviceFailed(QString::fromStdString(device.Message()));
    }
    return;
  }
  position_ = static_cast<int>(base_ + std::max<std::int64_t>(played, 0));
  emit PositionChanged(position_);
}

void Player::SetOutputFilter(core::dsp::OutputFilter filter) {
  if (filter == filter_) {
    return;
  }
  filter_ = filter;
  // The chip's output, whichever way the filter changed.
  if (state_ != PlaybackState::kStopped && IsChipOutput(source_)) {
    Stop();
  }
}

void Player::SetChipClock(int clock_hz) {
  if (clock_hz == clock_hz_) {
    return;
  }
  clock_hz_ = clock_hz;
  if (state_ != PlaybackState::kStopped && IsChipOutput(source_)) {
    Stop();
  }
}

void Player::SetOutputDevice(const QByteArray& id) {
  Stop();
  output_->SetDevice(id);
}

bool Player::IsFiltered(PlaybackSource source) const {
  return filter_ != core::dsp::OutputFilter::kNone && IsChipOutput(source);
}

int Player::SampleRateOf(PlaybackSource source) const {
  return IsChipOutput(source) ? core::synth::ChipSampleRate(clock_hz_)
                              : kSampleRate;
}

core::Status Player::StartOutput(int from) {
  // The rest of the buffer: the filtered output, or the samples
  // themselves.
  const bool filtered = !filtered_.empty();
  const int sample_rate = SampleRateOf(source_);
  const int source_rate =
      filtered ? sample_rate * core::dsp::kOutputOversampling : sample_rate;
  const std::vector<float> rest =
      filtered ? std::vector<float>(
                     filtered_.begin() + (static_cast<std::ptrdiff_t>(from) *
                                          core::dsp::kOutputOversampling),
                     filtered_.end())
               : ToFloat(std::span<const std::int16_t>(samples_).subspan(
                     static_cast<std::size_t>(from)));
  const int rate = output_->PlaybackRate(source_rate);
  std::vector<std::int16_t> buffer;
  if (rate == source_rate) {
    buffer = ToPcm16(rest);
  } else {
    core::StatusOr<std::vector<float>> resampled =
        resampler_->Resample(rest, source_rate, rate);
    if (!resampled.IsOk()) {
      SetState(PlaybackState::kStopped);
      return resampled.GetStatus();
    }
    buffer = ToPcm16(*resampled);
  }
  if (core::Status status = output_->Start(std::move(buffer), rate);
      !status.IsOk()) {
    core::Logger(core::kLogModuleGui)
        ->warn("playback failed: {}", status.Message());
    position_ = start_;
    SetState(PlaybackState::kStopped);
    return status;
  }
  base_ = from;
  position_ = from;
  started_us_ = clock_->NowMicroseconds();
  SetState(PlaybackState::kPlaying);
  emit PositionChanged(position_);
  return core::OkStatus();
}

void Player::SetState(PlaybackState state) {
  if (state == PlaybackState::kPlaying) {
    timer_.start();
  } else {
    timer_.stop();
  }
  if (state_ != state) {
    state_ = state;
    emit StateChanged();
  }
}

}  // namespace speak5220::app
