/*
 * File:        voice_activity.cpp
 * Module:      quality
 * Purpose:     Voice activity detection for the quality measures: the
 *              interface and its libfvad implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/voice_activity.h"

#include <fvad.h>

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <vector>

namespace speak5220::core::quality {
namespace {

constexpr int kSampleRate = 8000;
constexpr double kFullScale = 32767.0;

struct FvadDeleter {
  void operator()(Fvad* vad) const { fvad_free(vad); }
};

}  // namespace

FvadVoiceActivity::FvadVoiceActivity(int mode) : mode_(mode) {
  assert(mode >= kMinMode && mode <= kMaxMode);
}

StatusOr<std::vector<bool>> FvadVoiceActivity::SpeechFrames(
    std::span<const double> samples) {
  const std::unique_ptr<Fvad, FvadDeleter> vad(fvad_new());
  if (!vad) {
    return InternalError("libfvad: cannot create a detector");
  }
  if (fvad_set_mode(vad.get(), mode_) != 0 ||
      fvad_set_sample_rate(vad.get(), kSampleRate) != 0) {
    return InternalError("libfvad: cannot set the mode or the rate");
  }
  const std::size_t frames = samples.size() / kVoiceActivityFrame;
  std::vector<bool> speech(frames);
  std::array<std::int16_t, kVoiceActivityFrame> pcm{};
  for (std::size_t f = 0; f < frames; ++f) {
    const std::span<const double> frame =
        samples.subspan(f * kVoiceActivityFrame, kVoiceActivityFrame);
    std::ranges::transform(frame, pcm.begin(), [](double v) {
      return static_cast<std::int16_t>(
          std::lround(std::clamp(v * kFullScale, -kFullScale, kFullScale)));
    });
    const int result = fvad_process(vad.get(), pcm.data(), pcm.size());
    if (result < 0) {
      return InternalError("libfvad: cannot classify a frame");
    }
    speech[f] = result == 1;
  }
  return speech;
}

}  // namespace speak5220::core::quality
