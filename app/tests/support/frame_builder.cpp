/*
 * File:        frame_builder.cpp
 * Module:      test support
 * Purpose:     Builds frame bit streams by hand, and random frames, for the
 *              frame and emulator tests
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/frame_builder.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <random>
#include <vector>

#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"

namespace speak5220::testing {
namespace {

using core::lpc::Frame;
using core::lpc::FrameType;

int Uniform(std::mt19937& rng, int low, int high) {
  return std::uniform_int_distribution<int>(low, high)(rng);
}

int RandomK(std::mt19937& rng, std::size_t i) {
  return Uniform(rng, 0, (1 << core::lpc::kKBits.at(i)) - 1);
}

}  // namespace

void BitStream::Put(unsigned value, int width) {
  for (int i = width - 1; i >= 0; --i) {
    bits_.push_back(static_cast<int>((value >> i) & 1U));
  }
}

void BitStream::Voiced(int energy, int pitch, const std::array<int, 10>& k) {
  Put(static_cast<unsigned>(energy), 4);
  Put(0, 1);
  Put(static_cast<unsigned>(pitch), 6);
  for (std::size_t i = 0; i < k.size(); ++i) {
    Put(static_cast<unsigned>(k.at(i)), core::lpc::kKBits.at(i));
  }
}

void BitStream::Unvoiced(int energy, const std::array<int, 4>& k) {
  Put(static_cast<unsigned>(energy), 4);
  Put(0, 1);
  Put(0, 6);
  for (std::size_t i = 0; i < k.size(); ++i) {
    Put(static_cast<unsigned>(k.at(i)), core::lpc::kKBits.at(i));
  }
}

void BitStream::Repeat(int energy, int pitch) {
  Put(static_cast<unsigned>(energy), 4);
  Put(1, 1);
  Put(static_cast<unsigned>(pitch), 6);
}

std::vector<std::uint8_t> BitStream::Bytes() const {
  std::vector<std::uint8_t> bytes((bits_.size() + 7) / 8, 0);
  for (std::size_t i = 0; i < bits_.size(); ++i) {
    if (bits_[i] != 0) {
      bytes[i / 8] = static_cast<std::uint8_t>(bytes[i / 8] | (1U << (i % 8)));
    }
  }
  return bytes;
}

Frame RandomFrame(std::mt19937& rng, FrameType type) {
  switch (type) {
    case FrameType::kSilence:
      return Frame::Silence();
    case FrameType::kStop:
      return Frame::Stop();
    case FrameType::kRepeat:
      return Frame::Repeat(Uniform(rng, 1, 14), Uniform(rng, 0, 63));
    case FrameType::kUnvoiced: {
      std::array<int, 4> k = {};
      for (std::size_t i = 0; i < k.size(); ++i) {
        k.at(i) = RandomK(rng, i);
      }
      return Frame::Unvoiced(Uniform(rng, 1, 14), k);
    }
    case FrameType::kVoiced: {
      const int energy = Uniform(rng, 1, 14);
      const int pitch = Uniform(rng, 1, 63);
      std::array<int, 10> k = {};
      for (std::size_t i = 0; i < k.size(); ++i) {
        k.at(i) = RandomK(rng, i);
      }
      return Frame::Voiced(energy, pitch, k);
    }
  }
  return Frame::Silence();
}

Frame RandomFrame(std::mt19937& rng) {
  constexpr std::array<FrameType, 4> kTypes = {
      FrameType::kVoiced, FrameType::kUnvoiced, FrameType::kRepeat,
      FrameType::kSilence};
  return RandomFrame(rng,
                     kTypes.at(static_cast<std::size_t>(Uniform(rng, 0, 3))));
}

std::vector<Frame> SpeechLikeFrames(std::mt19937& rng, int count) {
  std::vector<Frame> frames = {
      Frame::Voiced(8, 30, {15, 15, 8, 8, 8, 8, 8, 4, 4, 4})};
  std::uniform_real_distribution<double> unit(0.0, 1.0);
  for (int i = 0; i < count; ++i) {
    const double r = unit(rng);
    if (r < 0.5) {
      const int energy = Uniform(rng, 6, 13);
      const int pitch = Uniform(rng, 20, 60);
      std::array<int, 10> k = {};
      k[0] = Uniform(rng, 8, 24);
      k[1] = Uniform(rng, 8, 24);
      for (std::size_t j = 2; j < 7; ++j) {
        k.at(j) = Uniform(rng, 4, 11);
      }
      for (std::size_t j = 7; j < 10; ++j) {
        k.at(j) = Uniform(rng, 2, 5);
      }
      frames.push_back(Frame::Voiced(energy, pitch, k));
    } else if (r < 0.7) {
      const int energy = Uniform(rng, 3, 8);
      const int k1 = Uniform(rng, 10, 24);
      const int k2 = Uniform(rng, 8, 24);
      const int k3 = Uniform(rng, 4, 11);
      const int k4 = Uniform(rng, 4, 11);
      frames.push_back(Frame::Unvoiced(energy, {k1, k2, k3, k4}));
    } else if (r < 0.9) {
      const int energy = Uniform(rng, 3, 12);
      const int pitch = Uniform(rng, 0, 1) == 0 ? 0 : Uniform(rng, 20, 60);
      frames.push_back(Frame::Repeat(energy, pitch));
    } else {
      frames.push_back(Frame::Silence());
    }
  }
  frames.push_back(Frame::Stop());
  return frames;
}

}  // namespace speak5220::testing
