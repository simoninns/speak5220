/*
 * File:        voice_activity.h
 * Module:      quality
 * Purpose:     Voice activity detection for the quality measures: the
 *              interface and its libfvad implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_VOICE_ACTIVITY_H_
#define SPEAK5220_CORE_QUALITY_VOICE_ACTIVITY_H_

#include <span>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::quality {

// The voice activity frame: 10 ms at 8 kHz, one of the lengths the WebRTC
// detector accepts.
inline constexpr int kVoiceActivityFrame = 80;

// Marks which frames of a signal hold speech. Implementations are
// thread-safe.
class IVoiceActivity {
 public:
  virtual ~IVoiceActivity() = default;

  // One flag per whole kVoiceActivityFrame of `samples` (8 kHz, -1..1),
  // true where the frame holds speech; a part frame at the end is left
  // out.
  virtual StatusOr<std::vector<bool>> SpeechFrames(
      std::span<const double> samples) = 0;
};

// IVoiceActivity over libfvad, the WebRTC detector as a C library, at 8 kHz
// (plan §3.3). Each call starts a fresh detector, so the result depends
// only on the samples. Samples are rounded to 16 bits and clipped, as the
// detector takes them. Thread-safe.
class FvadVoiceActivity : public IVoiceActivity {
 public:
  // The detector's aggressiveness: 0 (quality) to 3 (very aggressive).
  static constexpr int kMinMode = 0;
  static constexpr int kMaxMode = 3;

  // `mode` must be within kMinMode..kMaxMode.
  explicit FvadVoiceActivity(int mode);

  StatusOr<std::vector<bool>> SpeechFrames(
      std::span<const double> samples) override;

 private:
  int mode_;
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_VOICE_ACTIVITY_H_
