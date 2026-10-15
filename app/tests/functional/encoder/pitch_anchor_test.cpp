/*
 * File:        pitch_anchor_test.cpp
 * Module:      encoder tests
 * Purpose:     Functional tests of the encoder's pitch anchor (SPTK's RAPT):
 *              a voice under a boosted harmonic, and encodings on several
 *              threads at once
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <numbers>
#include <span>
#include <thread>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::encoder {
namespace {

constexpr int kRate = 8000;

// The `six` case the encoder's pitch anchor was tuned on: one
// second of a 117.6 Hz voice whose narrow first formant sits on its 4th
// harmonic, its period wandering by up to 3 % from cycle to cycle, between
// 0.2 s of silence; peak 0.5.
std::vector<double> BoostedHarmonicVowel() {
  std::vector<double> cycles;
  std::uint32_t state = 777;
  for (int c = 0; c <= kRate / 60; ++c) {
    state = (state * 1664525U) + 1013904223U;
    const double u = (static_cast<double>(state >> 8) / 16777216.0) - 0.5;
    cycles.push_back(68.0 * (1.0 + (0.06 * u)));
  }
  const double radius = 0.985;
  const double theta = 2.0 * std::numbers::pi * 470.6 / kRate;
  std::vector<double> x(static_cast<std::size_t>(kRate * 1.4), 0.0);
  const auto lead = static_cast<std::size_t>(kRate * 0.2);
  double phase = 0.0;
  double y1 = 0.0;
  double y2 = 0.0;
  for (int n = 0; n < kRate; ++n) {
    const double next =
        phase + (1.0 / cycles[static_cast<std::size_t>(n / 68)]);
    const double pulse = std::floor(next) > std::floor(phase) ? 1.0 : 0.0;
    phase = next;
    const double y =
        pulse + (2.0 * radius * std::cos(theta) * y1) - (radius * radius * y2);
    x[lead + static_cast<std::size_t>(n)] = y;
    y2 = y1;
    y1 = y;
  }
  double peak = 0.0;
  for (const double v : x) {
    peak = std::max(peak, std::abs(v));
  }
  for (double& v : x) {
    v *= 0.5 / peak;
  }
  return x;
}

TEST(PitchAnchorTest, EncoderCodesTheVoiceNotTheBoostedHarmonic) {
  const std::unique_ptr<IEncoder> encoder = MakeEncoder();
  const StatusOr<EncodeResult> result =
      encoder->Encode(BoostedHarmonicVowel(), kRate, {});
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  int sounding = 0;
  int voiced = 0;
  // The last two frames, the stop and the formant ringing on after the
  // last pulse, are left out.
  const std::span<const lpc::Frame> frames(result->frames);
  ASSERT_GT(frames.size(), 2U);
  for (const lpc::Frame& f : frames.first(frames.size() - 2)) {
    if (f.energy == 0) {
      continue;
    }
    ++sounding;
    if (f.pitch == 0) {
      continue;
    }
    ++voiced;
    // The period is the table's value plus one sample.
    const int period =
        lpc::kPitchTable.at(static_cast<std::size_t>(f.pitch)) + 1;
    EXPECT_NEAR(period, 68, 7) << "pitch index " << f.pitch;
  }
  EXPECT_GE(voiced * 10, sounding * 8);
}

TEST(PitchAnchorTest, EncodingsOnSeveralThreadsAtOnceMatchOne) {
  // RAPT runs inside every encoding; Optimise encodes on many threads.
  const std::vector<double> x = BoostedHarmonicVowel();
  const std::unique_ptr<IEncoder> encoder = MakeEncoder();
  const StatusOr<EncodeResult> alone = encoder->Encode(x, kRate, {});
  ASSERT_TRUE(alone.IsOk()) << alone.GetStatus().ToString();
  constexpr int kThreads = 8;
  std::vector<std::vector<lpc::Frame>> frames(kThreads);
  {
    std::vector<std::jthread> threads;
    threads.reserve(kThreads);
    for (int t = 0; t < kThreads; ++t) {
      threads.emplace_back([&, t] {
        const StatusOr<EncodeResult> r = encoder->Encode(x, kRate, {});
        if (r.IsOk()) {
          frames[static_cast<std::size_t>(t)] = r->frames;
        }
      });
    }
  }
  for (int t = 0; t < kThreads; ++t) {
    EXPECT_EQ(frames[static_cast<std::size_t>(t)], alone->frames)
        << "thread " << t;
  }
}

}  // namespace
}  // namespace speak5220::core::encoder
