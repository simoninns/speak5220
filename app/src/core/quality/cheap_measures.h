/*
 * File:        cheap_measures.h
 * Module:      quality
 * Purpose:     The fast quality measures for live feedback: how closely an
 *              encoding's synthesis follows the input's level and spectral
 *              shape, frame by frame
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_CHEAP_MEASURES_H_
#define SPEAK5220_CORE_QUALITY_CHEAP_MEASURES_H_

#include <span>

#include "core/encoder/encoder.h"
#include "core/lpc/frame.h"
#include "core/quality/quality_metric.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// investigation/encoder/evaluate.py measure()'s level and spectral
// measures, in dB, over the sounding frames within 30 dB of the loudest.
// Each frame's input (around its centre, times the encoder's gain) is
// compared with the reference synthesis around the sample where the
// frame's values are reached, so no alignment search is needed. About a
// millisecond per second of speech.
struct CheapMeasures {
  // Output level minus input level, per frame: the mean absolute
  // deviation from its median (lvMeanAbs), the median itself (lvBias) and
  // the 90th percentile of the absolute deviation (lvP90).
  double level_error = 0.0;
  double level_bias = 0.0;
  double level_p90 = 0.0;
  // The median over frames of the RMS difference between the input's and
  // the output's level-normalised 20-band mel spectra (specDist). NaN when
  // no frame qualifies.
  double spectral_distance = 0.0;
};

// The cheap measures of `frames`, analysed as `details`, synthesised by
// the reference synthesiser. Pure; thread-safe.
CheapMeasures MeasureCheap(const encoder::EncodeDetails& details,
                           std::span<const lpc::Frame> frames);

// IQualityMetric for the level error (CheapMeasures::level_error).
class LevelErrorMetric : public IQualityMetric {
 public:
  StatusOr<double> Measure(std::span<const double> source,
                           const encoder::EncodeResult& encoding) override;
  MetricDirection Direction() const override {
    return MetricDirection::kLowerIsBetter;
  }
};

// IQualityMetric for the spectral distance
// (CheapMeasures::spectral_distance).
class SpectralDistanceMetric : public IQualityMetric {
 public:
  StatusOr<double> Measure(std::span<const double> source,
                           const encoder::EncodeResult& encoding) override;
  MetricDirection Direction() const override {
    return MetricDirection::kLowerIsBetter;
  }
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_CHEAP_MEASURES_H_
