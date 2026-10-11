/*
 * File:        quality_metric.h
 * Module:      quality
 * Purpose:     IQualityMetric: how well an encoding reproduces its source,
 *              the measure Optimise ranks candidate encodings by
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_QUALITY_METRIC_H_
#define SPEAK5220_CORE_QUALITY_QUALITY_METRIC_H_

#include <span>

#include "core/encoder/encoder.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// Which way a metric's values improve.
enum class MetricDirection { kHigherIsBetter, kLowerIsBetter };

// Measures how well an encoding reproduces its source (plan §3.3). PESQ is
// the main measure; the cheap measures give live feedback. Implementations
// are deterministic and thread-safe.
class IQualityMetric {
 public:
  virtual ~IQualityMetric() = default;

  // Measures `encoding`, produced from `source` (8 kHz samples, -1..1, as
  // given to the encoder). Fails if the encoding holds no sound or the
  // measure cannot be computed.
  virtual StatusOr<double> Measure(std::span<const double> source,
                                   const encoder::EncodeResult& encoding) = 0;

  // Whether larger values are better.
  virtual MetricDirection Direction() const = 0;
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_QUALITY_METRIC_H_
