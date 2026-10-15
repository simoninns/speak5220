/*
 * File:        resampler_test.cpp
 * Module:      dsp tests
 * Purpose:     Functional tests of the libsamplerate resampler: level,
 *              length and no aliasing above 4 kHz
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/resampler.h"

#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

constexpr int kInputRate = 44100;
constexpr int kOutputRate = 8000;

double Rms(std::span<const float> x) {
  double sum = 0.0;
  for (const float v : x) {
    sum += static_cast<double>(v) * v;
  }
  return std::sqrt(sum / static_cast<double>(x.size()));
}

// The middle of a signal, away from the converter's start and end.
std::span<const float> Middle(const std::vector<float>& x) {
  const std::size_t edge = x.size() / 10;
  return std::span<const float>(x).subspan(edge, x.size() - 2 * edge);
}

TEST(SincResamplerTest, KeepsLevelAndLengthOfInBandTone) {
  std::vector<float> tone(kInputRate);  // one second of 1 kHz
  for (std::size_t i = 0; i < tone.size(); ++i) {
    tone[i] = 0.5F *
              static_cast<float>(std::sin(2.0 * std::numbers::pi * 1000.0 *
                                          static_cast<double>(i) / kInputRate));
  }
  SincResampler resampler;
  const StatusOr<std::vector<float>> out =
      resampler.Resample(tone, kInputRate, kOutputRate);
  ASSERT_TRUE(out.IsOk()) << out.GetStatus().ToString();
  EXPECT_NEAR(static_cast<double>(out->size()), kOutputRate, 2.0);
  EXPECT_NEAR(Rms(Middle(*out)), 0.5 / std::sqrt(2.0), 0.005);
}

// A swept sine entirely above the output's Nyquist frequency leaves
// nothing behind: no aliasing above 4 kHz.
TEST(SincResamplerTest, RemovesSweptSineAboveFourKilohertz) {
  constexpr double kStartHz = 4500.0;
  constexpr double kEndHz = 20000.0;
  constexpr double kSeconds = 2.0;
  const auto count = static_cast<std::size_t>(kSeconds * kInputRate);
  std::vector<float> sweep(count);
  for (std::size_t i = 0; i < count; ++i) {
    const double t = static_cast<double>(i) / kInputRate;
    const double phase =
        2.0 * std::numbers::pi *
        (kStartHz * t + (kEndHz - kStartHz) * t * t / (2.0 * kSeconds));
    sweep[i] = 0.5F * static_cast<float>(std::sin(phase));
  }
  SincResampler resampler;
  const StatusOr<std::vector<float>> out =
      resampler.Resample(sweep, kInputRate, kOutputRate);
  ASSERT_TRUE(out.IsOk()) << out.GetStatus().ToString();
  const double ratio_db =
      20.0 * std::log10(Rms(Middle(*out)) / Rms(Middle(sweep)));
  EXPECT_LT(ratio_db, -60.0);
}

TEST(SincResamplerTest, EqualRatesCopyInput) {
  const std::vector<float> input = {0.1F, -0.2F, 0.3F};
  const StatusOr<std::vector<float>> out =
      SincResampler().Resample(input, 8000, 8000);
  ASSERT_TRUE(out.IsOk());
  EXPECT_EQ(*out, input);
}

TEST(SincResamplerTest, RefusesBadRates) {
  const std::vector<float> input(10, 0.0F);
  EXPECT_EQ(SincResampler().Resample(input, 0, 8000).GetStatus().Code(),
            StatusCode::kInvalidArgument);
  EXPECT_EQ(SincResampler().Resample(input, 8000000, 8000).GetStatus().Code(),
            StatusCode::kOutOfRange);
}

}  // namespace
}  // namespace speak5220::core::dsp
