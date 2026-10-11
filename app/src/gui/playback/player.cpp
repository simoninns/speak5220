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

#include <QObject>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/resampler.h"
#include "core/util/clock.h"
#include "core/util/logging.h"
#include "core/util/status.h"
#include "gui/playback/audio_output.h"

namespace speak5220::app {

namespace {

constexpr std::int64_t kMicrosecondsPerSecond = 1000000;
// Full scale of a 16-bit sample.
constexpr float kFullScale = 32768.0F;

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
  const std::int64_t played = elapsed * kSampleRate / kMicrosecondsPerSecond;
  const auto end = static_cast<std::int64_t>(samples_.size());
  if (output_->IsFinished() || base_ + played >= end) {
    Stop();
    return;
  }
  position_ = static_cast<int>(base_ + std::max<std::int64_t>(played, 0));
  emit PositionChanged(position_);
}

core::Status Player::StartOutput(int from) {
  std::vector<std::int16_t> buffer(samples_.begin() + from, samples_.end());
  const int rate = output_->PlaybackRate(kSampleRate);
  if (rate != kSampleRate) {
    std::vector<float> scaled;
    scaled.reserve(buffer.size());
    for (const std::int16_t v : buffer) {
      scaled.push_back(static_cast<float>(v) / kFullScale);
    }
    core::StatusOr<std::vector<float>> resampled =
        resampler_->Resample(scaled, kSampleRate, rate);
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
