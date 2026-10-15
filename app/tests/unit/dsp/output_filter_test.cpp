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
  // The speech path's response: -0.4 dB at 1 kHz (0.3 dB from the
  // high-pass, 0.0 dB from the low-passes).
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

// scipy.signal.butter(1, 279, "highpass", fs=48000, output="sos") and
// scipy.signal.butter(2, 6950, fs=48000, output="sos").
TEST(OutputFilterTest, BbcMicroSectionsMatchScipysDesign) {
  const std::vector<SosSection> sections =
      OutputFilterSections(OutputFilter::kBbcMicro, kOutputRate);
  ASSERT_EQ(sections.size(), 3U);
  EXPECT_NEAR(sections[0].b[0], 0.9820650013779968, 1e-12);
  EXPECT_NEAR(sections[0].b[1], -0.9820650013779968, 1e-12);
  EXPECT_NEAR(sections[0].b[2], 0.0, 1e-12);
  EXPECT_NEAR(sections[0].a[0], -0.9641300027559936, 1e-12);
  EXPECT_NEAR(sections[0].a[1], 0.0, 1e-12);
  for (const std::size_t i : {1U, 2U}) {
    EXPECT_NEAR(sections[i].b[0], 0.12388333267370166, 1e-12);
    EXPECT_NEAR(sections[i].b[1], 0.24776666534740333, 1e-12);
    EXPECT_NEAR(sections[i].b[2], 0.12388333267370166, 1e-12);
    EXPECT_NEAR(sections[i].a[0], -0.7880350454339105, 1e-12);
    EXPECT_NEAR(sections[i].a[1], 0.2835683761287171, 1e-12);
  }
  EXPECT_TRUE(OutputFilterSections(OutputFilter::kNone, kOutputRate).empty());
}

// At another sample rate the analogue stage's corners stay put: a 100 Hz
// tone is cut as much whatever the rate the DAC runs at. Had the 279 Hz
// corner moved with the rate, to 314 Hz at 9 kHz, the peak would be about
// 10% lower; the tolerance allows for where the samples fall on the peak.
TEST(OutputFilterTest, BbcMicroKeepsItsCornersAtOtherRates) {
  constexpr int kFastRate = 9000;
  std::vector<float> samples(kFastRate);
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] =
        static_cast<float>(0.5 * std::sin(2.0 * std::numbers::pi * 100.0 *
                                          static_cast<double>(i) / kFastRate));
  }
  const std::vector<float> fast =
      FilterDacOutput(OutputFilter::kBbcMicro, samples, kFastRate);
  EXPECT_EQ(fast.size(), samples.size() * kOutputOversampling);
  const std::vector<float> nominal =
      FilterDacOutput(OutputFilter::kBbcMicro, Sine(100.0));
  EXPECT_NEAR(SettledPeak(fast), SettledPeak(nominal), 0.005);
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
