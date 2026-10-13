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
#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

#include "core/encoder/encoder_types.h"
#include "core/encoder/quantiser.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"

namespace speak5220::core::encoder {
namespace {

// AddFades(): unvoiced sound at this energy index (value 8) or more
// before silence or the end would stop audibly (§12.2), and is followed by
// one frame at about a third of its energy value.
constexpr int kFadeFromEnergy = 6;
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

// QuietPauses(): the parking frame's energy index, the quietest that is
// not silence (Table 8-4).
constexpr int kNeutralEnergy = 1;
// Its pitch when no voiced frame came before.
constexpr int kNeutralPitch = 1;

// Each K's index whose value is nearest zero (Table 8-6).
std::array<int, lpc::kKCount> NeutralK() {
  std::array<int, lpc::kKCount> k{};
  for (int i = 0; i < lpc::kKCount; ++i) {
    const std::span<const int> table = lpc::KTable(i);
    std::size_t best = 0;
    for (std::size_t j = 1; j < table.size(); ++j) {
      if (std::abs(table[j]) < std::abs(table[best])) {
        best = j;
      }
    }
    k.at(static_cast<std::size_t>(i)) = static_cast<int>(best);
  }
  return k;
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

std::vector<lpc::Frame> AddFades(std::span<const lpc::Frame> frames) {
  std::vector<lpc::Frame> faded;
  faded.reserve(frames.size() + 1);
  const std::size_t nfr = frames.size();
  std::size_t i = 0;
  while (i < nfr) {
    const lpc::Frame& f = frames[i];
    faded.push_back(f);
    ++i;
    const bool loud_unvoiced =
        IsSounding(f) && f.pitch == 0 && f.energy >= kFadeFromEnergy;
    if (loud_unvoiced &&
        (i == nfr || frames[i].energy == lpc::kSilenceEnergy)) {
      // In place of the silence frame that follows, or appended at the end.
      const int e =
          EnergyIndex(static_cast<double>(EnergyValue(f.energy)) / kFadeStep);
      faded.push_back(lpc::Frame::Repeat(e, 0));
      if (i < nfr) {
        ++i;
      }
    }
  }
  std::vector<lpc::Frame> out = GuardK510(faded);
  out.push_back(lpc::Frame::Stop());
  return out;
}

lpc::Frame NeutralFrame(int pitch) {
  return lpc::Frame::Voiced(kNeutralEnergy, pitch, NeutralK());
}

std::vector<lpc::Frame> QuietPauses(std::span<const lpc::Frame> frames) {
  std::vector<lpc::Frame> out(frames.begin(), frames.end());
  // The last full frame before each frame, whose K the chip holds, and
  // the last voiced pitch.
  std::optional<lpc::Frame> full;
  int pitch = kNeutralPitch;
  std::size_t j = 0;
  while (j < out.size()) {
    const lpc::FrameType kind = lpc::FrameKind(out[j]);
    if (kind == lpc::FrameType::kVoiced || kind == lpc::FrameType::kUnvoiced) {
      full = out[j];
    }
    if (out[j].pitch != 0) {
      pitch = out[j].pitch;
    }
    if (kind != lpc::FrameType::kSilence || !full.has_value()) {
      ++j;
      continue;
    }
    // A pause: silence frames [j, end).
    std::size_t end = j;
    while (end < out.size() &&
           lpc::FrameKind(out[end]) == lpc::FrameType::kSilence) {
      ++end;
    }
    const bool long_enough =
        end - j >= static_cast<std::size_t>(kMinQuietPause);
    const bool before_repeat =
        end < out.size() && lpc::FrameKind(out[end]) == lpc::FrameType::kRepeat;
    const bool full_unvoiced = full->pitch == 0;
    // A voiced repeat after an unvoiced frame keeps K5-K10 at zero, which
    // no full frame can say.
    const bool exact = !(before_repeat && full_unvoiced && out[end].pitch != 0);
    if (long_enough && exact) {
      out[j + kQuietPauseAt] = NeutralFrame(pitch);
      if (before_repeat) {
        const lpc::Frame& repeat = out[end];
        if (full_unvoiced) {
          out[end] = lpc::Frame::Unvoiced(
              repeat.energy, {full->k[0], full->k[1], full->k[2], full->k[3]});
        } else {
          out[end] = lpc::Frame::Voiced(repeat.energy, repeat.pitch, full->k);
          if (repeat.pitch == 0) {
            // An unvoiced repeat of a voiced frame: K1-K4 held, K5-K10
            // zeroed, which an unvoiced frame says exactly.
            out[end] = lpc::Frame::Unvoiced(
                repeat.energy,
                {full->k[0], full->k[1], full->k[2], full->k[3]});
          }
        }
      }
    }
    j = end;
  }
  return out;
}

}  // namespace speak5220::core::encoder
