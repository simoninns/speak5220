/*
 * File:        frame_endings.cpp
 * Module:      encoder
 * Purpose:     Turns the plan and energies into frames: repeats, fades
 *              before silence, the K5-K10 guard and the stop
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/frame_endings.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "core/encoder/encoder_types.h"
#include "core/encoder/quantiser.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"

namespace speak5220::core::encoder {
namespace {

// A fade ends once the energy value is this low (§12.2).
constexpr int kFadeEndValue = 2;
// Each fade frame has about this fraction of the energy before it.
constexpr double kFadeStep = 3.0;

bool IsSounding(const lpc::Frame& f) {
  return f.energy != lpc::kSilenceEnergy && f.energy != lpc::kStopEnergy;
}

lpc::Frame FullFrame(int energy, int pitch, std::span<const int> k) {
  if (pitch == 0) {
    std::array<int, lpc::kUnvoicedKCount> k4 = {};
    for (std::size_t i = 0; i < k4.size() && i < k.size(); ++i) {
      k4.at(i) = k[i];
    }
    return lpc::Frame::Unvoiced(energy, k4);
  }
  std::array<int, lpc::kVoicedKCount> k10 = {};
  for (std::size_t i = 0; i < k10.size() && i < k.size(); ++i) {
    k10.at(i) = k[i];
  }
  return lpc::Frame::Voiced(energy, pitch, k10);
}

int EnergyValue(int index) {
  return lpc::kEnergyTable.at(static_cast<std::size_t>(index));
}

}  // namespace

std::vector<lpc::Frame> ToFrames(std::span<const PlanEntry> plan,
                                 std::span<const int> energies) {
  assert(plan.size() == energies.size());
  std::vector<lpc::Frame> frames;
  struct Stored {
    std::vector<int> k;
    bool voiced = false;
  };
  std::optional<Stored> stored;
  for (std::size_t j = 0; j < plan.size(); ++j) {
    const PlanEntry& p = plan[j];
    const int e = energies[j];
    if (p.kind == PlanKind::kSilence || e == 0) {
      frames.push_back(lpc::Frame::Silence());
      continue;
    }
    const bool voiced = p.pitch != 0;
    if (p.kind == PlanKind::kRepeat && stored.has_value() && stored->k == p.k &&
        stored->voiced == voiced) {
      frames.push_back(lpc::Frame::Repeat(e, p.pitch));
    } else {
      frames.push_back(FullFrame(e, p.pitch, p.k));
      stored = Stored{.k = p.k, .voiced = voiced};
    }
  }
  const auto first = std::ranges::find_if(frames, [](const lpc::Frame& f) {
    return f.energy != lpc::kSilenceEnergy;
  });
  frames.erase(frames.begin(), first);
  while (!frames.empty() && frames.back().energy == lpc::kSilenceEnergy) {
    frames.pop_back();
  }
  return frames;
}

std::vector<lpc::Frame> GuardK510(std::span<const lpc::Frame> frames) {
  std::vector<lpc::Frame> out;
  out.reserve(frames.size());
  bool voiced_seen = false;
  for (const lpc::Frame& f : frames) {
    if (f.energy == lpc::kSilenceEnergy && !voiced_seen) {
      out.push_back(lpc::Frame::Repeat(1, 0));
      continue;
    }
    if (IsSounding(f) && !f.repeat && f.pitch != 0) {
      voiced_seen = true;
    }
    out.push_back(f);
  }
  return out;
}

std::vector<lpc::Frame> AddFades(std::span<const lpc::Frame> frames,
                                 bool fade_unvoiced) {
  std::vector<lpc::Frame> out;
  if (!fade_unvoiced) {
    out = GuardK510(frames);
    out.push_back(lpc::Frame::Stop());
    return out;
  }
  std::vector<lpc::Frame> faded;
  const std::size_t nfr = frames.size();
  std::size_t i = 0;
  while (i < nfr) {
    const lpc::Frame& f = frames[i];
    faded.push_back(f);
    const bool unvoiced = IsSounding(f) && f.pitch == 0;
    if (unvoiced &&
        (i + 1 == nfr || frames[i + 1].energy == lpc::kSilenceEnergy)) {
      // Replace the following silence frames (or append, at the end).
      int e = f.energy;
      std::size_t j = i + 1;
      while (EnergyValue(e) > kFadeEndValue) {
        e = EnergyIndex(static_cast<double>(EnergyValue(e)) / kFadeStep);
        const lpc::Frame fade = lpc::Frame::Repeat(e, 0);
        if (j >= nfr || frames[j].energy == lpc::kSilenceEnergy) {
          faded.push_back(fade);
          ++j;
        } else {
          break;
        }
      }
      i = std::max(i + 1, std::min(j, nfr));
      continue;
    }
    ++i;
  }
  out = GuardK510(faded);
  out.push_back(lpc::Frame::Stop());
  return out;
}

}  // namespace speak5220::core::encoder
