/*
 * File:        frame_analyser.cpp
 * Module:      encoder
 * Purpose:     Finds the speech, places the frames and measures each
 *              frame's level and spectrum
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/frame_analyser.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/iir_filter.h"
#include "core/dsp/lpc_analysis.h"
#include "core/encoder/encoder_filters.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/util/logging.h"

namespace speak5220::core::encoder {
namespace {

// The level envelope: 10 ms blocks (half 40) every 2.5 ms.
constexpr int kBlockStep = 20;
constexpr int kBlockHalf = 40;
// Frame levels for the loudest frame: 25 ms around every 200th sample from
// sample 100.
constexpr int kFrameHalf = 100;
constexpr double kFloorPercentile = 5.0;
constexpr double kFloorFactor = 2.0;
// c0 is the onset plus this (the sound starts at output sample 203, §8.5).
constexpr int kOnsetToCentre = kReachedSample - 203;
// The first frame's window starts no earlier than this many samples before
// the onset.
constexpr int kFirstWindowLead = 40;
// A white-noise floor on r[0] that keeps Levinson stable.
constexpr double kNoiseFloor = 1.0001;

// The level setter's peak windows are 2 x 110 + 1 samples.
constexpr int kPeakHalf = 110;

// The Hann window of 2 half + 1 values for FrameRms(); the common sizes
// are built once.
std::vector<double> RmsWindow(int half) {
  return dsp::InnerHannWindow((2 * half) + 1);
}

double WeightedRms(std::span<const double> x, int centre, int half,
                   std::span<const double> w, double weight) {
  const int a = centre - half;
  const int b = centre + half + 1;
  const int lo = std::max(0, a);
  const int hi = std::min(static_cast<int>(x.size()), b);
  if (hi <= lo) {
    return 0.0;
  }
  double dot = 0.0;
  for (int i = lo; i < hi; ++i) {
    const double s = x[static_cast<std::size_t>(i)];
    dot += w[static_cast<std::size_t>(i - a)] * (s * s);
  }
  return std::sqrt(dot / weight);
}

std::vector<double> LagWindow(double hz) {
  std::vector<double> w(kAutocorrelationSize);
  for (int i = 0; i < kAutocorrelationSize; ++i) {
    const double t = 2.0 * std::numbers::pi * hz * static_cast<double>(i) /
                     static_cast<double>(kEncoderSampleRate);
    w[static_cast<std::size_t>(i)] = std::exp(-0.5 * (t * t));
  }
  return w;
}

}  // namespace

double FrameRms(std::span<const double> x, int centre, int half) {
  static const std::vector<double> kBlock = RmsWindow(kBlockHalf);
  static const std::vector<double> kFrame = RmsWindow(kFrameHalf);
  static const std::vector<double> kPeak = RmsWindow(kPeakHalf);
  static const double kBlockWeight = dsp::PairwiseSum(kBlock);
  static const double kFrameWeight = dsp::PairwiseSum(kFrame);
  static const double kPeakWeight = dsp::PairwiseSum(kPeak);
  switch (half) {
    case kBlockHalf:
      return WeightedRms(x, centre, half, kBlock, kBlockWeight);
    case kFrameHalf:
      return WeightedRms(x, centre, half, kFrame, kFrameWeight);
    case kPeakHalf:
      return WeightedRms(x, centre, half, kPeak, kPeakWeight);
    default: {
      const std::vector<double> w = RmsWindow(half);
      return WeightedRms(x, centre, half, w, dsp::PairwiseSum(w));
    }
  }
}

std::optional<Segmentation> FrameAnalyser::Segment(
    std::span<const double> samples, const EncoderOptions& options) {
  const auto log = Logger(kLogModuleEncoder);
  Segmentation seg;
  seg.x = HighPass60Hz(samples);
  const auto n = static_cast<int>(seg.x.size());
  std::vector<double> blk;
  for (int c = 0; c < n; c += kBlockStep) {
    blk.push_back(FrameRms(seg.x, c, kBlockHalf));
  }
  const auto positive = [](double v) { return v > 0.0; };
  if (std::ranges::none_of(blk, positive)) {
    log->debug("analysis: no sound");
    return std::nullopt;
  }
  double loud = 0.0;
  bool first = true;
  for (int c = kFrameHalf; c < std::max(n, kFrameHalf + 1);
       c += kFrameSamples) {
    const double level = FrameRms(seg.x, c, kFrameHalf);
    loud = first ? level : std::max(loud, level);
    first = false;
  }
  const double floor = dsp::Percentile(blk, kFloorPercentile);
  const double gate = std::max(loud * std::pow(10.0, -options.gate_db / 20.0),
                               floor * kFloorFactor);
  int first_above = -1;
  int last_above = -1;
  for (std::size_t i = 0; i < blk.size(); ++i) {
    if (blk[i] > gate) {
      if (first_above < 0) {
        first_above = static_cast<int>(i);
      }
      last_above = static_cast<int>(i);
    }
  }
  if (first_above < 0) {
    log->debug("analysis: nothing above the gate {:.6g}", gate);
    return std::nullopt;
  }
  seg.gate = gate;
  seg.loud = loud;
  seg.onset = first_above * kBlockStep;
  seg.offset = last_above * kBlockStep;
  seg.c0 = seg.onset + kOnsetToCentre;
  const int span = seg.offset + kFrameHalf - seg.c0;
  // Python's floor division; span is never negative here.
  const int count = std::max(1, (span / kFrameSamples) + 1);
  for (int j = 0; j < count; ++j) {
    seg.centres.push_back(seg.c0 + (kFrameSamples * j));
  }
  log->debug(
      "analysis: gate {:.6g}, loud {:.6g}, onset {}, offset {}, {} frames",
      gate, loud, seg.onset, seg.offset, count);
  return seg;
}

std::vector<FrameFeatures> FrameAnalyser::Features(
    const Segmentation& segmentation, std::span<const FrameVoicing> voicing,
    const EncoderOptions& options) {
  assert(voicing.size() == segmentation.centres.size());
  const std::vector<double>& x = segmentation.x;
  const int win = options.window;
  const auto uwin = static_cast<std::size_t>(win);
  const std::vector<double> ham = dsp::HammingWindow(win);
  const std::vector<double> lagw = LagWindow(options.lag_window_hz);
  const std::vector<double> xp =
      dsp::FirFilter(PreEmphasisFilter(options.pre_emphasis), x);
  const std::vector<double> xl = LoudnessHighPass(x);
  // Sample i is at i + win of the padded signals.
  std::vector<double> pad(x.size() + (2 * uwin) + kFrameSamples, 0.0);
  std::vector<double> padp(pad.size(), 0.0);
  std::ranges::copy(x, pad.begin() + win);
  std::ranges::copy(xp, padp.begin() + win);

  std::vector<FrameFeatures> feats;
  std::vector<double> seg_v(uwin);
  std::vector<double> seg_u(uwin);
  for (std::size_t j = 0; j < segmentation.centres.size(); ++j) {
    const int c = segmentation.centres[j];
    FrameFeatures f;
    f.centre = c;
    f.level = FrameRms(x, c);
    f.loudness = FrameRms(xl, c);
    f.voiced = voicing[j].voiced;
    f.period = voicing[j].period;
    const int ac =
        j > 0 ? c
              : std::max(c, segmentation.onset + (win / 2) - kFirstWindowLead);
    const int start = ac - (win / 2) + win;
    const auto a0 = static_cast<std::size_t>(start);
    assert(a0 + uwin <= pad.size());
    for (std::size_t i = 0; i < uwin; ++i) {
      seg_v[i] = padp[a0 + i] * ham[i];
      seg_u[i] = pad[a0 + i] * ham[i];
    }
    const std::vector<double> rv = dsp::Autocorrelation(seg_v, kLpcOrder);
    const std::vector<double> ru = dsp::Autocorrelation(seg_u, kLpcOrder);
    for (std::size_t i = 0; i < kAutocorrelationSize; ++i) {
      f.rv.at(i) = rv[i] * lagw[i];
      f.ru.at(i) = ru[i] * lagw[i];
    }
    f.rv[0] *= kNoiseFloor;
    f.ru[0] *= kNoiseFloor;
    f.gated = f.level <= segmentation.gate;
    feats.push_back(f);
  }
  return feats;
}

}  // namespace speak5220::core::encoder
