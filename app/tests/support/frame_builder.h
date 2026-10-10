/*
 * File:        frame_builder.h
 * Module:      test support
 * Purpose:     Builds frame bit streams by hand, and random frames, for the
 *              frame and emulator tests
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_FRAME_BUILDER_H_
#define SPEAK5220_TESTS_SUPPORT_FRAME_BUILDER_H_

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "core/lpc/frame.h"

namespace speak5220::testing {

// A bit stream assembled field by field: fields MSB first, bytes filled
// from bit 0 (speak5220 datasheet §8.1). Re-implements the "bits" builder
// of test_tms5220.c, independently of FrameCodec.
class BitStream {
 public:
  void Put(unsigned value, int width);
  void Voiced(int energy, int pitch, const std::array<int, 10>& k);
  void Unvoiced(int energy, const std::array<int, 4>& k);
  void Repeat(int energy, int pitch);
  void Silence() { Put(0, 4); }
  void Stop() { Put(15, 4); }

  // The bits so far, one per element, in stream order.
  const std::vector<int>& Bits() const { return bits_; }
  // The bytes, the last one padded with zeros.
  std::vector<std::uint8_t> Bytes() const;

 private:
  std::vector<int> bits_;
};

// The neutral K indices of test_tms5220.c.
inline constexpr std::array<int, 10> kNeutralVoicedK = {20, 10, 7, 8, 8,
                                                        8,  8,  4, 4, 4};
inline constexpr std::array<int, 4> kNeutralUnvoicedK = {20, 10, 7, 8};

// A random frame of the given kind, or of any kind but stop. Re-implements
// test_tms5220enc.py randomFrame().
core::lpc::Frame RandomFrame(std::mt19937& rng, core::lpc::FrameType type);
core::lpc::Frame RandomFrame(std::mt19937& rng);

// Random frames in the range of speech, starting voiced and ending in a
// stop. Re-implements test_tms5220enc.py speechLike().
std::vector<core::lpc::Frame> SpeechLikeFrames(std::mt19937& rng, int count);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_FRAME_BUILDER_H_
