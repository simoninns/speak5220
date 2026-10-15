/*
 * File:        peak_cache.h
 * Module:      waveform
 * Purpose:     Lowest and highest sample over any range of a waveform, from
 *              precomputed blocks at several resolutions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_PEAK_CACHE_H_
#define SPEAK5220_GUI_WAVEFORM_PEAK_CACHE_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace speak5220::app {

// The lowest and highest sample of a range.
struct Peak {
  float low = 0.0F;
  float high = 0.0F;

  friend bool operator==(const Peak& a, const Peak& b) = default;
};

// The waveform view draws one vertical line per pixel, from the
// lowest to the highest sample under it. The cache keeps each block's
// peak at block sizes of 16, 64, 256, ... samples, so a range takes the
// largest blocks that fit inside it and finer ones at its ends: O(log n)
// per range, O(n) memory and construction. Immutable; thread-safe.
class PeakCache {
 public:
  // Samples per block at the finest level, and the factor between levels.
  static constexpr std::size_t kFinestBlock = 16;
  static constexpr std::size_t kLevelFactor = 4;

  PeakCache() = default;
  explicit PeakCache(std::vector<float> samples);

  std::size_t Size() const { return samples_.size(); }
  std::span<const float> Samples() const { return samples_; }

  // The peak of the samples in [first, last), clamped to the samples.
  // Zero for a range with no samples in it.
  Peak Range(std::int64_t first, std::int64_t last) const;

 private:
  Peak Scan(std::size_t first, std::size_t last) const;
  Peak RangeAt(int level, std::size_t first, std::size_t last) const;

  std::vector<float> samples_;
  // levels_[k] holds blocks of kFinestBlock * kLevelFactor^k samples; the
  // last block of a level may be shorter.
  std::vector<std::vector<Peak>> levels_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_PEAK_CACHE_H_
