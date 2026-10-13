/*
 * File:        loudness.cpp
 * Module:      quality
 * Purpose:     The loudness figures of the chip's output: sounding RMS,
 *              peak, clipping, lattice wraps, and the level through the
 *              BBC Micro's speech path and A-weighted
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/loudness.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/iir_filter.h"
#include "core/encoder/encoder_filters.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/level_setter.h"
#include "core/lpc/frame.h"

namespace speak5220::core::quality {
namespace {

constexpr double kSampleRate = encoder::kEncoderSampleRate;

// IEC 61672-1, Annex E: the A-weighting's pole frequencies in Hz, and its
// gain at 1 kHz in dB, which the normalisation takes back.
constexpr double kAPole1Hz = 20.598997;
constexpr double kAPole2Hz = 107.65265;
constexpr double kAPole3Hz = 737.86223;
constexpr double kAPole4Hz = 12194.217;
constexpr double kAGainAt1kHzDb = 1.9997;

double AngularHz(double hz) { return 2.0 * std::numbers::pi * hz; }

// The bilinear transform (s = 2 fs (z - 1) / (z + 1), unwarped, as
// scipy.signal.bilinear()) of (b1 s + b0) / (s + w).
dsp::SosSection FirstOrder(double b1, double b0, double w) {
  const double c = 2.0 * kSampleRate;
  const double norm = c + w;
  return {.b = {((b1 * c) + b0) / norm, (b0 - (b1 * c)) / norm, 0.0},
          .a = {(w - c) / norm, 0.0},
          .zi = {}};
}

std::vector<double> Filter(std::span<const dsp::SosSection> sections,
                           std::span<const double> x) {
  std::vector<double> y(x.begin(), x.end());
  std::vector<double> state(2 * sections.size(), 0.0);
  dsp::SosFilterInPlace(sections, state, y);
  return y;
}

double Rms(std::span<const double> x) {
  if (x.empty()) {
    return 0.0;
  }
  double sum = 0.0;
  for (const double v : x) {
    sum += v * v;
  }
  return std::sqrt(sum / static_cast<double>(x.size()));
}

}  // namespace

std::vector<double> BbcHighPass(std::span<const double> x) {
  return encoder::LoudnessHighPass(x);
}

std::vector<double> AWeighted(std::span<const double> x) {
  // (w4^2 s^4 g) / ((s + w4)^2 (s + w1)^2 (s + w3) (s + w2)), as six
  // first-order sections; the gain g goes into the first.
  const double w1 = AngularHz(kAPole1Hz);
  const double w2 = AngularHz(kAPole2Hz);
  const double w3 = AngularHz(kAPole3Hz);
  const double w4 = AngularHz(kAPole4Hz);
  const double gain = std::pow(10.0, kAGainAt1kHzDb / 20.0);
  const std::array<dsp::SosSection, 6> sections = {
      FirstOrder(0.0, w4 * gain, w4), FirstOrder(0.0, w4, w4),
      FirstOrder(1.0, 0.0, w1),       FirstOrder(1.0, 0.0, w1),
      FirstOrder(1.0, 0.0, w2),       FirstOrder(1.0, 0.0, w3)};
  return Filter(sections, x);
}

Loudness MeasureLoudness(std::span<const int> codes,
                         std::span<const lpc::Frame> frames,
                         int wrapped_samples) {
  Loudness out;
  out.wrapped_samples = wrapped_samples;
  int clipped = 0;
  for (const int code : codes) {
    const int magnitude = std::abs(code);
    out.peak = std::max(out.peak, magnitude);
    clipped += magnitude > kDacLimit ? 1 : 0;
  }
  if (!codes.empty()) {
    out.clipped =
        static_cast<double>(clipped) / static_cast<double>(codes.size());
  }

  // Every frame up to the first stop has a level.
  int levelled = 0;
  for (const lpc::Frame& frame : frames) {
    if (lpc::FrameKind(frame) == lpc::FrameType::kStop) {
      break;
    }
    ++levelled;
  }
  const encoder::OutputLevels levels =
      encoder::MeasureOutputLevels(codes, levelled);
  double sum = 0.0;
  int sounding = 0;
  for (int j = 0; j < levelled; ++j) {
    const lpc::FrameType type =
        lpc::FrameKind(frames[static_cast<std::size_t>(j)]);
    if (type == lpc::FrameType::kVoiced || type == lpc::FrameType::kUnvoiced ||
        type == lpc::FrameType::kRepeat) {
      const double level = levels.level[static_cast<std::size_t>(j)];
      sum += level * level;
      ++sounding;
    }
  }
  out.sounding_rms = sounding > 0 ? std::sqrt(sum / sounding) : 0.0;

  if (std::cmp_greater(codes.size(), kLoudnessStartSample)) {
    const std::vector<double> talk(codes.begin() + kLoudnessStartSample,
                                   codes.end());
    out.high_passed_rms = Rms(BbcHighPass(talk));
    out.a_weighted_rms = Rms(AWeighted(talk));
  }
  return out;
}

}  // namespace speak5220::core::quality
