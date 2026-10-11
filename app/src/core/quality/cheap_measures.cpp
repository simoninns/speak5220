/*
 * File:        cheap_measures.cpp
 * Module:      quality
 * Purpose:     The fast quality measures for live feedback: how closely an
 *              encoding's synthesis follows the input's level and spectral
 *              shape, frame by frame
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/cheap_measures.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <span>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/fft.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_analyser.h"
#include "core/quality/output_alignment.h"
#include "core/synth/reference_synthesizer.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

namespace {

// evaluate.py: 20 mel bands over 256-point spectra.
constexpr int kBands = 20;
constexpr int kNfft = 256;
constexpr int kBins = (kNfft / 2) + 1;
constexpr int kHalfSegment = kNfft / 2;
// Frames more than this below the loudest sounding frame are left out.
constexpr double kRangeDb = 30.0;
// Levels are floored at this RMS before taking dB.
constexpr double kLevelFloor = 1e-3;
constexpr double kBandFloor = 1e-9;
constexpr double kP90 = 90.0;

std::size_t Index(int i) { return static_cast<std::size_t>(i); }

const std::vector<double>& Bank() {
  static const std::vector<double> kBank = MelFilterBank(kBands);
  return kBank;
}

const std::vector<double>& Hamming() {
  static const std::vector<double> kWindow = dsp::HammingWindow(kNfft);
  return kWindow;
}

// evaluate.py bandsDb(): x[max(0, centre - 128), centre + 128) times
// `scale`, zero-padded at the end to 256 samples, Hamming-windowed: the
// mel band levels in dB.
std::vector<double> BandsDb(std::span<const double> x, int centre,
                            double scale) {
  std::vector<double> seg(kNfft, 0.0);
  const int from = std::max(0, centre - kHalfSegment);
  const int to = std::min(static_cast<int>(x.size()), centre + kHalfSegment);
  for (int i = from; i < to; ++i) {
    seg[Index(i - from)] = x[Index(i)] * scale;
  }
  const std::vector<double>& w = Hamming();
  for (std::size_t i = 0; i < seg.size(); ++i) {
    seg[i] *= w[i];
  }
  dsp::PocketFft fft;
  const std::vector<std::complex<double>> bins = fft.Forward(seg);
  const std::vector<double>& fb = Bank();
  std::vector<double> bands(kBands);
  for (int band = 0; band < kBands; ++band) {
    double sum = 0.0;
    for (int k = 0; k < kBins; ++k) {
      const double m = std::abs(bins[Index(k)]);
      sum += fb[Index((band * kBins) + k)] * (m * m);
    }
    bands[Index(band)] = 10 * std::log10(sum + kBandFloor);
  }
  return bands;
}

}  // namespace

CheapMeasures MeasureCheap(const encoder::EncodeDetails& details,
                           std::span<const lpc::Frame> frames) {
  const synth::ReferenceSynthesis synthesis =
      synth::ReferenceSynthesizer::Synthesize(frames);
  const std::vector<double> codes(synthesis.codes.begin(),
                                  synthesis.codes.end());
  const std::span<const double> x = details.x;
  const int n = static_cast<int>(details.plan.size());

  // Each frame's input and output level.
  std::vector<double> in_db(Index(n));
  std::vector<double> out_db(Index(n));
  std::vector<bool> sounding(Index(n));
  double loudest = -std::numeric_limits<double>::infinity();
  for (int j = 0; j < n; ++j) {
    const double a =
        encoder::FrameRms(x, details.centres[Index(j)]) * details.gain;
    const double o = encoder::FrameRms(
        codes, encoder::kReachedSample + (encoder::kFrameSamples * j));
    in_db[Index(j)] = 20 * std::log10(std::max(a, kLevelFloor));
    out_db[Index(j)] = 20 * std::log10(std::max(o, kLevelFloor));
    sounding[Index(j)] =
        details.plan[Index(j)].kind != encoder::PlanKind::kSilence;
    if (sounding[Index(j)]) {
      loudest = std::max(loudest, in_db[Index(j)]);
    }
  }
  if (loudest == -std::numeric_limits<double>::infinity()) {
    loudest = 0.0;
  }

  std::vector<double> level_err;
  std::vector<double> distances;
  for (int j = 0; j < n; ++j) {
    if (!sounding[Index(j)] || in_db[Index(j)] <= loudest - kRangeDb) {
      continue;
    }
    level_err.push_back(out_db[Index(j)] - in_db[Index(j)]);
    std::vector<double> bi =
        BandsDb(x, details.centres[Index(j)], details.gain);
    std::vector<double> bo = BandsDb(
        codes, encoder::kReachedSample + (encoder::kFrameSamples * j), 1.0);
    const double mean_in = dsp::Mean(bi);
    const double mean_out = dsp::Mean(bo);
    std::vector<double> d2(kBands);
    for (int b = 0; b < kBands; ++b) {
      const double d = (bi[Index(b)] - mean_in) - (bo[Index(b)] - mean_out);
      d2[Index(b)] = d * d;
    }
    distances.push_back(std::sqrt(dsp::Mean(d2)));
  }

  CheapMeasures m;
  if (!level_err.empty()) {
    const double median = dsp::Median(level_err);
    std::vector<double> deviation(level_err.size());
    for (std::size_t i = 0; i < level_err.size(); ++i) {
      deviation[i] = std::fabs(level_err[i] - median);
    }
    m.level_error = dsp::Mean(deviation);
    m.level_bias = median;
    m.level_p90 = dsp::Percentile(deviation, kP90);
  }
  m.spectral_distance = distances.empty()
                            ? std::numeric_limits<double>::quiet_NaN()
                            : dsp::Median(distances);
  return m;
}

StatusOr<double> LevelErrorMetric::Measure(
    std::span<const double> /*source*/, const encoder::EncodeResult& encoding) {
  if (!encoding.details.has_value()) {
    return FailedPreconditionError("the encoding holds no sound");
  }
  return MeasureCheap(*encoding.details, encoding.frames).level_error;
}

StatusOr<double> SpectralDistanceMetric::Measure(
    std::span<const double> /*source*/, const encoder::EncodeResult& encoding) {
  if (!encoding.details.has_value()) {
    return FailedPreconditionError("the encoding holds no sound");
  }
  const double d =
      MeasureCheap(*encoding.details, encoding.frames).spectral_distance;
  if (std::isnan(d)) {
    return FailedPreconditionError("no frame is loud enough to compare");
  }
  return d;
}

}  // namespace speak5220::core::quality
