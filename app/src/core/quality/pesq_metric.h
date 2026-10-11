/*
 * File:        pesq_metric.h
 * Module:      quality
 * Purpose:     PESQ of an encoding as the chip speaks it: the emulator's
 *              output lined up with the source and scored
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_PESQ_METRIC_H_
#define SPEAK5220_CORE_QUALITY_PESQ_METRIC_H_

#include <span>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/lpc/frame.h"
#include "core/quality/quality_metric.h"
#include "core/synth/synth.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// One PESQ measurement of an encoding.
struct PesqMeasurement {
  // ITU-T P.862.1 MOS-LQO.
  double score = 0.0;
  // The samples by which the chip's output was delayed to line up with the
  // source.
  int lag = 0;
  // The chip's output, from its first non-idle code to its last.
  std::vector<double> output;
};

// Plan §3.3: PESQ as investigation/tools/speechqual.py measures it. The
// frames are spoken by the emulator through the VSM path, the output
// (codes, idle at 0) is cut to its sound, lined up with the source after
// the encoder's 60 Hz high-pass (AlignOutput()) and scored against it
// with ScorePesq(). Thread-safe if the synth is.
class PesqMetric : public IQualityMetric {
 public:
  // `synth` must outlive the metric.
  explicit PesqMetric(synth::ISynth& synth);

  // The MOS-LQO of encoding.frames.
  StatusOr<double> Measure(std::span<const double> source,
                           const encoder::EncodeResult& encoding) override;
  MetricDirection Direction() const override {
    return MetricDirection::kHigherIsBetter;
  }

  // The measurement of `frames` (ending in a stop) against `source`.
  StatusOr<PesqMeasurement> MeasureFrames(std::span<const double> source,
                                          std::span<const lpc::Frame> frames);

 private:
  synth::ISynth* synth_;
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_PESQ_METRIC_H_
