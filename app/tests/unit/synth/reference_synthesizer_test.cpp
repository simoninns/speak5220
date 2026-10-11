/*
 * File:        reference_synthesizer_test.cpp
 * Module:      synth tests
 * Purpose:     Unit tests of the encoder's reference-algorithm synthesiser:
 *              start and end of speech, and the sign of the lattice
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/reference_synthesizer.h"

#include <algorithm>
#include <cstddef>
#include <random>
#include <vector>

#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/synth/synth.h"
#include "gtest/gtest.h"
#include "support/frame_builder.h"

namespace speak5220::core::synth {
namespace {

using lpc::Frame;

std::vector<std::size_t> NonIdle(const std::vector<int>& codes) {
  std::vector<std::size_t> out;
  for (std::size_t i = 0; i < codes.size(); ++i) {
    if (codes[i] != -1) {
      out.push_back(i);
    }
  }
  return out;
}

// Re-implements test_tms5220enc.py Synthesis.testStartAndEnd.
TEST(ReferenceSynthesizerTest, StartsAndEndsWhereTheDatasheetSays) {
  const Frame voiced = Frame::Voiced(10, 40, {20, 15, 8, 8, 8, 8, 8, 4, 4, 4});
  std::vector<Frame> frames(4, voiced);
  frames.push_back(Frame::Stop());
  const std::vector<int> codes = ReferenceSynthesizer::Synthesize(frames).codes;
  const std::vector<std::size_t> non_idle = NonIdle(codes);
  ASSERT_FALSE(non_idle.empty());
  // §7: 206-207 samples for PROM words.
  EXPECT_GE(non_idle.front(), 205U);
  EXPECT_LT(non_idle.front(), 210U);
  // Unvoiced: the energy is computed in PC 0 A of IP 0 (sample 201) and
  // reaches the filter two samples later (§8.5), when the noise shows at
  // once.
  const Frame unvoiced = Frame::Unvoiced(10, {20, 15, 8, 8});
  std::vector<Frame> unvoiced_frames(4, unvoiced);
  unvoiced_frames.push_back(Frame::Stop());
  const std::vector<int> unvoiced_codes =
      ReferenceSynthesizer::Synthesize(unvoiced_frames).codes;
  ASSERT_FALSE(NonIdle(unvoiced_codes).empty());
  EXPECT_EQ(NonIdle(unvoiced_codes).front(), 203U);
  // It ends after 25 idle samples: talking for 5 frames (the stop
  // included), then a short decay.
  ASSERT_GE(codes.size(), 25U);
  EXPECT_TRUE(std::all_of(codes.end() - 25, codes.end(),
                          [](int c) { return c == -1; }));
  EXPECT_LT(non_idle.back(), 5U * 200 + 60);
  EXPECT_GT(non_idle.back(), 5U * 200);
}

// Re-implements test_tms5220enc.py Synthesis.testIdleStop.
TEST(ReferenceSynthesizerTest, StopAloneStaysIdle) {
  const std::vector<Frame> frames = {Frame::Stop()};
  EXPECT_TRUE(NonIdle(ReferenceSynthesizer::Synthesize(frames).codes).empty());
}

TEST(ReferenceSynthesizerTest, RunningOutOfFramesEndsLikeStop) {
  const Frame voiced = Frame::Voiced(9, 30, {20, 15, 8, 8, 8, 8, 8, 4, 4, 4});
  const std::vector<Frame> open = {voiced, voiced};
  const std::vector<Frame> closed = {voiced, voiced, Frame::Stop()};
  EXPECT_EQ(ReferenceSynthesizer::Synthesize(open).codes,
            ReferenceSynthesizer::Synthesize(closed).codes);
}

// Re-implements test_tms5220enc.py SignConvention.testNegativeK1IsLowPass:
// in the chip's synthesis a large negative K1 gives a low-frequency output.
TEST(ReferenceSynthesizerTest, NegativeK1IsLowPass) {
  auto lag1 = [](int k1_index) {
    const Frame frame = Frame::Unvoiced(8, {k1_index, 15, 8, 6});
    std::vector<Frame> frames(6, frame);
    frames.push_back(Frame::Stop());
    const std::vector<int> codes =
        ReferenceSynthesizer::Synthesize(frames).codes;
    std::vector<double> c(codes.begin() + 400, codes.begin() + 1200);
    double mean = 0.0;
    for (const double v : c) {
      mean += v;
    }
    mean /= static_cast<double>(c.size());
    double num = 0.0;
    double den = 0.0;
    for (std::size_t i = 0; i < c.size(); ++i) {
      den += (c[i] - mean) * (c[i] - mean);
      if (i + 1 < c.size()) {
        num += (c[i] - mean) * (c[i + 1] - mean);
      }
    }
    return num / den;
  };
  EXPECT_GT(lag1(5), 0.8);    // K1 = -491
  EXPECT_LT(lag1(30), -0.3);  // K1 = +411
}

// The synthesiser matches the emulator's VSM path from the first frame
// read (re-implements test_tms5220enc.py
// Synthesis.testAgainstReferenceEmulator against the C++ emulator).
TEST(ReferenceSynthesizerTest, MatchesEmulatorVsmPath) {
  std::mt19937 rng(6);
  Synth synth;
  for (int trial = 0; trial < 3; ++trial) {
    const std::vector<Frame> frames =
        speak5220::testing::SpeechLikeFrames(rng, 30);
    const StatusOr<std::vector<std::uint8_t>> packed =
        lpc::FrameCodec::Pack(frames);
    ASSERT_TRUE(packed.IsOk());
    const StatusOr<SynthResult> emulated =
        synth.Synthesize(*packed, SynthPath::kVsm);
    ASSERT_TRUE(emulated.IsOk());
    const std::vector<int> codes =
        ReferenceSynthesizer::Synthesize(frames).codes;
    ASSERT_LE(codes.size(), emulated->codes.size());
    EXPECT_TRUE(std::equal(codes.begin(), codes.end(), emulated->codes.begin()))
        << "trial " << trial;
  }
}

}  // namespace
}  // namespace speak5220::core::synth
