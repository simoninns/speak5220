/*
 * File:        peak_cache.cpp
 * Module:      waveform
 * Purpose:     Lowest and highest sample over any range of a waveform, from
 *              precomputed blocks at several resolutions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/peak_cache.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace speak5220::app {

namespace {

Peak Combine(const Peak& a, const Peak& b) {
  return {.low = std::min(a.low, b.low), .high = std::max(a.high, b.high)};
}

}  // namespace

PeakCache::PeakCache(std::vector<float> samples)
    : samples_(std::move(samples)) {
  // Each level from the samples (the finest) or from the level below.
  std::size_t block = kFinestBlock;
  while (block < samples_.size()) {
    std::vector<Peak> level;
    level.reserve((samples_.size() + block - 1) / block);
    if (levels_.empty()) {
      for (std::size_t i = 0; i < samples_.size(); i += block) {
        level.push_back(Scan(i, std::min(i + block, samples_.size())));
      }
    } else {
      const std::vector<Peak>& below = levels_.back();
      for (std::size_t i = 0; i < below.size(); i += kLevelFactor) {
        Peak peak = below[i];
        for (std::size_t j = i + 1;
             j < std::min(i + kLevelFactor, below.size()); ++j) {
          peak = Combine(peak, below[j]);
        }
        level.push_back(peak);
      }
    }
    levels_.push_back(std::move(level));
    block *= kLevelFactor;
  }
}

Peak PeakCache::Range(std::int64_t first, std::int64_t last) const {
  const auto size = static_cast<std::int64_t>(samples_.size());
  first = std::clamp<std::int64_t>(first, 0, size);
  last = std::clamp<std::int64_t>(last, 0, size);
  if (first >= last) {
    return {};
  }
  return RangeAt(static_cast<int>(levels_.size()) - 1,
                 static_cast<std::size_t>(first),
                 static_cast<std::size_t>(last));
}

Peak PeakCache::Scan(std::size_t first, std::size_t last) const {
  Peak peak{.low = samples_[first], .high = samples_[first]};
  for (std::size_t i = first + 1; i < last; ++i) {
    peak.low = std::min(peak.low, samples_[i]);
    peak.high = std::max(peak.high, samples_[i]);
  }
  return peak;
}

Peak PeakCache::RangeAt(int level, std::size_t first, std::size_t last) const {
  if (level < 0) {
    return Scan(first, last);
  }
  std::size_t block = kFinestBlock;
  for (int k = 0; k < level; ++k) {
    block *= kLevelFactor;
  }
  // The whole blocks inside [first, last); a block that runs past the end
  // of the samples is whole if the range reaches that end.
  const std::size_t begin = (first + block - 1) / block;
  std::size_t end = last / block;
  if (last == samples_.size()) {
    end = levels_[static_cast<std::size_t>(level)].size();
  }
  if (begin >= end) {
    return RangeAt(level - 1, first, last);
  }
  const std::vector<Peak>& blocks = levels_[static_cast<std::size_t>(level)];
  Peak peak = blocks[begin];
  for (std::size_t i = begin + 1; i < end; ++i) {
    peak = Combine(peak, blocks[i]);
  }
  if (first < begin * block) {
    peak = Combine(peak, RangeAt(level - 1, first, begin * block));
  }
  const std::size_t covered = std::min(end * block, samples_.size());
  if (covered < last) {
    peak = Combine(peak, RangeAt(level - 1, covered, last));
  }
  return peak;
}

}  // namespace speak5220::app
