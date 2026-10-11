/*
 * File:        reference_synthesizer.cpp
 * Module:      synth
 * Purpose:     The encoder's own synthesiser: the datasheet's reference
 *              algorithm run directly on a frame list
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/reference_synthesizer.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <vector>

#include "core/emulator/chip_arithmetic.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"

namespace speak5220::core::synth {
namespace {

using emulator::BoothClosedForm;
using emulator::NoiseStep;
using emulator::Wrap14;

constexpr int kSamplesPerFrame = 200;
constexpr int kSlotsPerSample = 20;
constexpr int kSamplesPerInterpolation = 25;
// §7: the noise register's state at slot 0 of sample 1 for a Speak.
constexpr std::uint32_t kSpeakNoiseState = 0x00EB;
// §8.7 and §8.9, as in the emulator.
constexpr std::array<int, 8> kInterpolationShift = {0, 3, 3, 3, 2, 2, 1, 1};
constexpr std::array<int, 12> kFilterDelay = {2, 3, 1, 1, 1, 1,
                                              1, 1, 1, 1, 1, 1};
constexpr std::int32_t kNoiseExcitation = 64;
constexpr std::int32_t kExcitationScale = 64;
constexpr int kCodeShift = 4;
constexpr int kLastChirp = 51;
constexpr int kIdleCode = -1;
constexpr int kIdleSamplesAtEnd = 25;
// Settling passes for the idle filter state (§8.8: u1 = -9).
constexpr int kIdleSettlingPasses = 16;
// Frames of margin in the sample limit: the stop's frame and the decay.
constexpr int kExtraFrames = 3;
constexpr int kExtraSamples = 400;

struct QueuedWrite {
  int due = 0;
  int parameter = 0;
  int value = 0;
};

bool IsSilence(int energy) {
  return energy == lpc::kSilenceEnergy || energy == lpc::kStopEnergy;
}

void ApplyDue(std::vector<QueuedWrite>& queue, int n,
              std::array<int, lpc::kParameterCount>& seen) {
  std::size_t kept = 0;
  for (const QueuedWrite& write : queue) {
    if (write.due <= n) {
      seen[static_cast<std::size_t>(write.parameter)] = write.value;
    } else {
      queue[kept++] = write;
    }
  }
  queue.resize(kept);
}

}  // namespace

ReferenceSynthesis ReferenceSynthesizer::Synthesize(
    std::span<const lpc::Frame> frames,
    const std::optional<std::array<int, lpc::kParameterCount>>&
        initial_indices) {
  using Parameters = std::array<int, lpc::kParameterCount>;
  Parameters index = initial_indices.value_or(Parameters{});  // §7
  Parameters current = {};
  Parameters seen = {};
  std::vector<QueuedWrite> queue;
  queue.reserve(2 * std::size_t{lpc::kParameterCount});
  bool talking = false;
  bool requested = false;
  bool zeroing = false;
  bool unvoiced = false;
  bool last_unvoiced = false;
  bool inhibit = false;
  bool hold = false;
  bool old_silence = false;
  bool old_unvoiced = false;
  int loads = 0;
  int count = 0;
  std::uint32_t noise = kSpeakNoiseState;
  std::size_t next_frame = 0;
  std::optional<lpc::Frame> pending;
  std::array<std::int32_t, 12> u = {};
  std::array<std::int32_t, 11> b = {};

  // Idle filter state: energy and K zero give u1 = -9 (§8.8).
  for (int pass = 0; pass < kIdleSettlingPasses; ++pass) {
    u[11] = 1;
    for (std::size_t i = 10; i >= 1; --i) {
      u[i] = u[i + 1] - 1;
    }
    for (std::size_t i = 10; i >= 2; --i) {
      b[i] = b[i - 1] + 1;
    }
    b[1] = u[1];
  }
  for (std::size_t i = 1; i < b.size(); ++i) {
    b[i] = Wrap14(b[i]);
  }

  ReferenceSynthesis out;
  bool ended = false;
  int idle_run = 0;
  bool old_unvoiced_previous = old_unvoiced;
  const int limit =
      kSamplesPerFrame * (static_cast<int>(frames.size()) + kExtraFrames) +
      kExtraSamples;
  out.codes.reserve(static_cast<std::size_t>(limit));
  for (int n = 0; n < limit; ++n) {
    const int s = n % kSamplesPerFrame;
    int ip = 7;
    int pc = 12;
    bool is_a = false;
    if (s != 0) {
      const int q = s - 1;
      ip = q / kSamplesPerInterpolation;
      const int w = q % kSamplesPerInterpolation;
      pc = w >> 1;
      is_a = (w & 1) == 0 && pc < 12;
    }
    const bool old_unvoiced_now = old_unvoiced;

    // Frame reads (sample 0 of each frame).
    if (s == 0 && (n == 0 || (talking && requested))) {
      if (n == 0) {
        zeroing = true;
        unvoiced = true;
        loads = 0;
        requested = true;
      }
      if (next_frame < frames.size()) {
        pending = frames[next_frame++];
      } else {
        pending = lpc::Frame::Stop();  // out of data: ends like a stop
      }
    }

    // Step 1.
    ApplyDue(queue, n, seen);

    bool excitation_unvoiced = false;
    if (talking) {
      // Load a frame in PC 12 of IP 0.
      if (ip == 0 && pc == 12 && pending) {
        const lpc::Frame frame = *pending;
        pending.reset();
        ++loads;
        const int e = frame.energy;
        if (!(e == lpc::kSilenceEnergy && loads == 1)) {
          zeroing = false;
          unvoiced = false;
        }
        index[0] = e;
        if (e == lpc::kStopEnergy) {
          requested = false;
        }
        if (!IsSilence(e)) {
          index[1] = frame.pitch;
          if (frame.repeat) {
            unvoiced = frame.pitch == 0 || last_unvoiced;
          } else {
            unvoiced = frame.pitch == 0;
            last_unvoiced = unvoiced;
            // Unvoiced frames load K1-K4 only.
            const auto carried =
                static_cast<std::size_t>(lpc::FrameKCount(frame));
            for (std::size_t j = 0; j < carried; ++j) {
              index[2 + j] = frame.k[j];
            }
          }
        }
        const bool silence = IsSilence(index[0]);
        const bool now_unvoiced = index[1] == 0;
        inhibit = (old_unvoiced != now_unvoiced) || (old_silence && !silence) ||
                  (old_unvoiced && silence);
      }
      // Parameter update in the A sample of each PC.
      if (is_a) {
        const int i = pc;
        const auto at = static_cast<std::size_t>(i);
        const bool now = inhibit && ip != 0;
        const int target = (zeroing || (unvoiced && i >= 6))
                               ? 0
                               : lpc::ParameterValue(i, index[at]);
        const int step =
            now ? 0
                : (target - current[at]) >>
                      kInterpolationShift[static_cast<std::size_t>(ip)];
        bool zero = i <= 5 ? zeroing : unvoiced;
        if (i >= 6 && now) {
          zero = false;
        }
        int value = 0;
        if (zero && zeroing) {
          current[at] += step;
        } else if (zero) {
          current[at] = 0;
        } else {
          current[at] += step;
          value = current[at];
        }
        queue.push_back({n + kFilterDelay[at], i, value});
        if (ip == 0 && pc == 1) {
          hold = false;
        }
      }
      excitation_unvoiced = old_unvoiced_previous;
    } else {
      if (is_a) {
        queue.push_back(
            {n + kFilterDelay[static_cast<std::size_t>(pc)] - 1, pc, 0});
      }
      excitation_unvoiced = old_unvoiced;
    }

    ApplyDue(queue, n, seen);

    // Excitation, in units of 1/128.
    std::int32_t x = 0;
    if (excitation_unvoiced) {
      x = (noise & 1U) != 0 ? -kNoiseExcitation : kNoiseExcitation;
    } else {
      x = lpc::kChirpTable[static_cast<std::size_t>(
          std::min(count, kLastChirp))];
    }

    // Lattice (§8.8), with the multiplier in closed form.
    bool wrapped = false;
    std::int32_t raw = BoothClosedForm(kExcitationScale * x, seen[0]);
    std::int32_t value = Wrap14(raw);
    wrapped = value != raw;
    std::int32_t peak = std::abs(raw);
    u[11] = value;
    for (std::size_t i = 10; i >= 1; --i) {
      raw = u[i + 1] - BoothClosedForm(b[i], seen[i + 1]);
      value = Wrap14(raw);
      wrapped = wrapped || value != raw;
      peak = std::max(peak, std::abs(raw));
      u[i] = value;
    }
    for (std::size_t i = 10; i >= 2; --i) {
      raw = b[i - 1] + BoothClosedForm(u[i - 1], seen[i]);
      value = Wrap14(raw);
      wrapped = wrapped || value != raw;
      peak = std::max(peak, std::abs(raw));
      b[i] = value;
    }
    b[1] = u[1];
    if (wrapped) {
      ++out.wrapped_samples;
    }
    out.peak_internal = std::max(out.peak_internal, peak);
    const int code = u[1] >> kCodeShift;
    out.codes.push_back(code);

    // End of the period.
    old_unvoiced_previous = old_unvoiced_now;
    if (s == 0) {
      if (inhibit && talking) {
        hold = true;
      }
      old_silence = IsSilence(index[0]);
      old_unvoiced = index[1] == 0;
      if (talking && !requested) {
        ended = true;
      }
      talking = (talking || n == 0) ? requested : false;
    }
    if (talking) {
      ++count;
      if (count >= seen[1] + 1 || hold) {
        count = 0;
      }
    }
    if (n >= 1) {
      for (int k = 0; k < kSlotsPerSample; ++k) {
        noise = NoiseStep(noise);
      }
    }
    if (ended && !talking) {
      idle_run = code == kIdleCode ? idle_run + 1 : 0;
      if (idle_run >= kIdleSamplesAtEnd) {
        break;
      }
    }
  }
  return out;
}

}  // namespace speak5220::core::synth
