/*
 * File:        resampler.h
 * Module:      dsp
 * Purpose:     Sample-rate conversion: the interface and its libsamplerate
 *              implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_RESAMPLER_H_
#define SPEAK5220_CORE_DSP_RESAMPLER_H_

#include <span>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::dsp {

// Converts mono audio between sample rates. Implementations are
// thread-safe.
class IResampler {
 public:
  virtual ~IResampler() = default;

  // Returns `input` at `output_rate`. Equal rates return a copy.
  virtual StatusOr<std::vector<float>> Resample(std::span<const float> input,
                                                int input_rate,
                                                int output_rate) = 0;
};

// IResampler over libsamplerate's best sinc converter,
// SRC_SINC_BEST_QUALITY. Stateless; thread-safe.
class SincResampler : public IResampler {
 public:
  StatusOr<std::vector<float>> Resample(std::span<const float> input,
                                        int input_rate,
                                        int output_rate) override;
};

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_RESAMPLER_H_
