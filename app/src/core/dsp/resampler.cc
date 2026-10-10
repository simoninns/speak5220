/*
 * File:        resampler.cc
 * Module:      dsp
 * Purpose:     Sample-rate conversion: the interface and its libsamplerate
 *              implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/resampler.h"

#include <samplerate.h>

#include <cmath>
#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace speak5220::core::dsp {
namespace {

// libsamplerate accepts ratios within 1/256..256.
constexpr double kMaxRatio = 256.0;
// Output room beyond the exact length, for the converter's rounding.
constexpr std::size_t kOutputMargin = 16;

}  // namespace

StatusOr<std::vector<float>> SincResampler::Resample(
    std::span<const float> input, int input_rate, int output_rate) {
  if (input_rate <= 0 || output_rate <= 0) {
    return InvalidArgumentError("sample rates must be positive");
  }
  if (input_rate == output_rate) {
    return std::vector<float>(input.begin(), input.end());
  }
  const double ratio =
      static_cast<double>(output_rate) / static_cast<double>(input_rate);
  if (ratio > kMaxRatio || ratio < 1.0 / kMaxRatio) {
    return OutOfRangeError("cannot resample from " +
                           std::to_string(input_rate) + " Hz to " +
                           std::to_string(output_rate) + " Hz");
  }
  if (input.empty()) {
    return std::vector<float>();
  }
  const auto capacity = static_cast<std::size_t>(std::ceil(
                            static_cast<double>(input.size()) * ratio)) +
                        kOutputMargin;
  std::vector<float> output(capacity);
  SRC_DATA data = {};
  data.data_in = input.data();
  data.input_frames = static_cast<decltype(data.input_frames)>(input.size());
  data.data_out = output.data();
  data.output_frames = static_cast<decltype(data.output_frames)>(output.size());
  data.src_ratio = ratio;
  data.end_of_input = 1;
  const int error = src_simple(&data, SRC_SINC_BEST_QUALITY, 1);
  if (error != 0) {
    return InternalError(std::string("libsamplerate: ") + src_strerror(error));
  }
  output.resize(static_cast<std::size_t>(data.output_frames_gen));
  return output;
}

}  // namespace speak5220::core::dsp
