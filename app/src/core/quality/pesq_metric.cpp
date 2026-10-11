/*
 * File:        pesq_metric.cpp
 * Module:      quality
 * Purpose:     PESQ of an encoding as the chip speaks it: the emulator's
 *              output lined up with the source and scored
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pesq_metric.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/encoder/encoder_filters.h"
#include "core/lpc/frame_codec.h"
#include "core/quality/output_alignment.h"
#include "core/quality/pesq.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

namespace {

// The chip's idle output code.
constexpr int kIdleCode = -1;

}  // namespace

PesqMetric::PesqMetric(synth::ISynth& synth) : synth_(&synth) {
  assert(synth_ != nullptr);
}

StatusOr<double> PesqMetric::Measure(std::span<const double> source,
                                     const encoder::EncodeResult& encoding) {
  StatusOr<PesqMeasurement> m = MeasureFrames(source, encoding.frames);
  if (!m.IsOk()) {
    return m.GetStatus();
  }
  return m->score;
}

StatusOr<PesqMeasurement> PesqMetric::MeasureFrames(
    std::span<const double> source, std::span<const lpc::Frame> frames) {
  StatusOr<std::vector<std::uint8_t>> packed = lpc::FrameCodec::Pack(frames);
  if (!packed.IsOk()) {
    return packed.GetStatus();
  }
  StatusOr<synth::SynthResult> spoken =
      synth_->Synthesize(*packed, synth::SynthPath::kVsm);
  if (!spoken.IsOk()) {
    return spoken.GetStatus();
  }
  // The sound: from the first non-idle code to the last, with idle at 0
  // (speechqual.py emulate() and measure()).
  const std::vector<int>& codes = spoken->codes;
  const auto first =
      std::ranges::find_if(codes, [](int c) { return c != kIdleCode; });
  if (first == codes.end()) {
    return FailedPreconditionError("the encoding makes no sound");
  }
  const auto last = std::ranges::find_if(std::views::reverse(codes),
                                         [](int c) { return c != kIdleCode; });
  PesqMeasurement m;
  m.output.reserve(static_cast<std::size_t>(last.base() - first));
  for (auto it = first; it != last.base(); ++it) {
    m.output.push_back(static_cast<double>(*it) + 1.0);
  }

  const std::vector<double> ref = encoder::HighPass60Hz(source);
  AlignedOutput aligned = AlignOutput(ref, m.output);
  m.lag = aligned.lag;
  StatusOr<PesqScore> score = ScorePesq(ref, aligned.degraded);
  if (!score.IsOk()) {
    return score.GetStatus();
  }
  m.score = score->mos_lqo;
  Logger(kLogModuleQuality)
      ->debug("PESQ {:.3f} at lag {} ({} output samples)", m.score, m.lag,
              m.output.size());
  return m;
}

}  // namespace speak5220::core::quality
