/*
 * File:        pcm24.h
 * Module:      audio
 * Purpose:     Quantisation of samples to the 24-bit PCM grid a project
 *              stores its audio on
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_AUDIO_PCM24_H_
#define SPEAK5220_CORE_AUDIO_PCM24_H_

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>

namespace speak5220::core::audio {

// A project stores its 8 kHz audio as 24-bit PCM. Full scale
// is 2^23, so a sample k / 2^23 is exact in a float (24-bit mantissa) and
// survives a save and a load unchanged, which keeps the encoding cache's
// hashes stable.
inline constexpr std::int32_t kPcm24Min = -8388608;  // -2^23
inline constexpr std::int32_t kPcm24Max = 8388607;   // 2^23 - 1
inline constexpr double kPcm24Scale = 8388608.0;     // 2^23

// The 24-bit code for `sample` (-1..1): rounded to the nearest code, halves
// away from zero, and clipped. Non-finite samples become 0.
inline std::int32_t ToPcm24(float sample) {
  const double scaled = static_cast<double>(sample) * kPcm24Scale;
  if (!std::isfinite(scaled)) {
    return 0;
  }
  const double clipped = std::clamp(scaled, static_cast<double>(kPcm24Min),
                                    static_cast<double>(kPcm24Max));
  return static_cast<std::int32_t>(std::lround(clipped));
}

// The sample a 24-bit code stands for; exact.
inline float FromPcm24(std::int32_t code) {
  return static_cast<float>(static_cast<double>(code) / kPcm24Scale);
}

// Moves every sample to the nearest point of the 24-bit grid.
inline void QuantiseToPcm24(std::span<float> samples) {
  for (float& sample : samples) {
    sample = FromPcm24(ToPcm24(sample));
  }
}

}  // namespace speak5220::core::audio

#endif  // SPEAK5220_CORE_AUDIO_PCM24_H_
