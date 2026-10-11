/*
 * File:        output_filter_test.cpp
 * Module:      dsp tests
 * Purpose:     Unit tests of the output filters: the held DAC staircase,
 *              the BBC Micro's response and the filters' names
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/dsp/output_filter.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace speak5220::core::dsp {
namespace {

using ::testing::ElementsAre;
using ::testing::Optional;

// One second of a sine at `hz` and amplitude 0.5, at 8 kHz.
std::vector<float> Sine(double hz) {
  std::vector<float> samples(8000);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] =
        static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * hz *
                                          static_cast<double>(i) / 8000.0));
  }
  return samples;
}

// The peak of the second half of `samples`, once the filters have settled.
double SettledPeak(std::span<const float> samples) {
  double peak = 0.0;
  for (const float v : samples.subspan(samples.size() / 2)) {
    peak = std::max(peak, static_cast<double>(std::abs(v)));
  }
  return peak;
}

TEST(OutputFilterTest, KeysNameEachFilter) {
  for (const OutputFilterInfo& info : OutputFilters()) {
    EXPECT_THAT(OutputFilterFromKey(info.key), Optional(info.filter));
    EXPECT_EQ(InfoFor(info.filter).key, info.key);
  }
  EXPECT_EQ(OutputFilters().front().filter, OutputFilter::kNone);
  EXPECT_EQ(OutputFilterFromKey("bbc-micro"), OutputFilter::kBbcMicro);
  EXPECT_EQ(OutputFilterFromKey("gramophone"), std::nullopt);
}

TEST(OutputFilterTest, NoFilterHoldsEachSampleForItsPeriod) {
  const std::vector<float> samples = {0.25F, -0.5F};
  const std::vector<float> output =
      FilterDacOutput(OutputFilter::kNone, samples);
  EXPECT_THAT(output, ElementsAre(0.25F, 0.25F, 0.25F, 0.25F, 0.25F, 0.25F,
                                  -0.5F, -0.5F, -0.5F, -0.5F, -0.5F, -0.5F));
  EXPECT_EQ(FilterSamples(OutputFilter::kNone, samples), samples);
}

TEST(OutputFilterTest, EmptyInputGivesEmptyOutput) {
  EXPECT_TRUE(FilterDacOutput(OutputFilter::kBbcMicro, {}).empty());
  EXPECT_TRUE(FilterSamples(OutputFilter::kBbcMicro, {}).empty());
}

TEST(OutputFilterTest, BbcMicroRemovesAnOffset) {
  // Silence, then a step of 0.5 held for a second: the 279 Hz high-pass
  // takes it back to zero.
  std::vector<float> samples(8000, 0.5F);
  samples.front() = 0.0F;
  const std::vector<float> output =
      FilterSamples(OutputFilter::kBbcMicro, samples);
  ASSERT_EQ(output.size(), samples.size());
  EXPECT_GT(output[2], 0.4F);
  EXPECT_NEAR(output.back(), 0.0F, 1e-6);
}

TEST(OutputFilterTest, BbcMicroPassesTheSpeechBand) {
  // The audio-output-stage page's table: -0.4 dB at 1 kHz.
  const std::vector<float> output =
      FilterDacOutput(OutputFilter::kBbcMicro, Sine(1000.0));
  EXPECT_NEAR(20.0 * std::log10(SettledPeak(output) / 0.5), -0.4, 0.3);
}

TEST(OutputFilterTest, BbcMicroCutsTheBass) {
  // -9.4 dB at 100 Hz, from the 279 Hz high-pass.
  const std::vector<float> output =
      FilterDacOutput(OutputFilter::kBbcMicro, Sine(100.0));
  EXPECT_NEAR(20.0 * std::log10(SettledPeak(output) / 0.5), -9.4, 0.3);
}

TEST(OutputFilterTest, FilterSamplesKeepsOneValuePerSample) {
  const std::vector<float> samples = Sine(440.0);
  EXPECT_EQ(FilterSamples(OutputFilter::kBbcMicro, samples).size(),
            samples.size());
  EXPECT_EQ(FilterDacOutput(OutputFilter::kBbcMicro, samples).size(),
            samples.size() * kOutputOversampling);
}

}  // namespace
}  // namespace speak5220::core::dsp
