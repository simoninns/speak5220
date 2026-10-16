/*
 * File:        output_pin_test.cpp
 * Module:      synth tests
 * Purpose:     Unit tests of the chip's two outputs: the SPEAKER pin's DAC,
 *              its clipping and its uneven steps, and the I/O pin's
 *              unclipped codes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/output_pin.h"

#include <cmath>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/synth/synth.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace speak5220::core::synth {
namespace {

using ::testing::ElementsAre;
using ::testing::Optional;

// Codes from the whole 10-bit range, as 16-bit samples.
const std::vector<std::int16_t> kSamples = {
    -512 * kSampleScale, -129 * kSampleScale, -128 * kSampleScale, 0,
    127 * kSampleScale,  128 * kSampleScale,  511 * kSampleScale};

// `v` rounded to a sample.
std::int16_t Rounded(double v) {
  return static_cast<std::int16_t>(std::lround(v));
}

TEST(OutputPinTest, KeysNameEachPin) {
  for (const OutputPinInfo& info : OutputPins()) {
    EXPECT_THAT(OutputPinFromKey(info.key), Optional(info.pin));
    EXPECT_EQ(InfoFor(info.pin).key, info.key);
  }
  EXPECT_EQ(OutputPins().front().pin, OutputPin::kSpeaker);
  EXPECT_EQ(OutputPinFromKey("io"), OutputPin::kIo);
  EXPECT_EQ(OutputPinFromKey("headphones"), std::nullopt);
}

TEST(OutputPinTest, SpeakerClipsToTheDacRange) {
  // Datasheet §8.2: codes above +127 give +127's output, and below -128
  // give -128's. The full range is kept: -128 and +127 give themselves.
  const std::vector<std::int16_t> out =
      PinSamples(OutputPin::kSpeaker, kSamples);
  EXPECT_EQ(out[0], -128 * kSampleScale);
  EXPECT_EQ(out[1], -128 * kSampleScale);
  EXPECT_EQ(out[2], -128 * kSampleScale);
  EXPECT_EQ(out[4], 127 * kSampleScale);
  EXPECT_EQ(out[5], 127 * kSampleScale);
  EXPECT_EQ(out[6], 127 * kSampleScale);
}

TEST(OutputPinTest, SpeakerGivesEachCodeItsDacOutput) {
  const std::vector<std::int16_t> codes = {-1 * kSampleScale, 0,
                                           5 * kSampleScale};
  EXPECT_THAT(PinSamples(OutputPin::kSpeaker, codes),
              ElementsAre(Rounded(DacOutput(-1) * kSampleScale),
                          Rounded(DacOutput(0) * kSampleScale),
                          Rounded(DacOutput(5) * kSampleScale)));
}

TEST(OutputPinTest, DacStepsBetweenMinusOneAndZeroBySixSteps) {
  // Datasheet §8.2, Table 8-3: the step from -1 to 0, where every input
  // bit changes, is 5.85 ideal steps.
  EXPECT_NEAR(DacOutput(0) - DacOutput(-1), 5.85, 0.2);
  // So -1 and 0 sit about 3.5 and 2.5 steps either side of zero.
  EXPECT_NEAR(DacOutput(-1), -3.5, 0.2);
  EXPECT_NEAR(DacOutput(0), 2.5, 0.2);
}

TEST(OutputPinTest, DacStepsElsewhereAreNearlyOneStep) {
  // A step that changes only the low bits is close to an ideal step.
  EXPECT_NEAR(DacOutput(2) - DacOutput(1), 1.0, 0.2);
  EXPECT_NEAR(DacOutput(-6) - DacOutput(-7), 1.0, 0.2);
  EXPECT_DOUBLE_EQ(DacOutput(-128), -128.0);
  EXPECT_DOUBLE_EQ(DacOutput(127), 127.0);
}

TEST(OutputPinTest, DacOutputRisesWithTheCode) {
  for (int code = -128; code < 127; ++code) {
    EXPECT_LT(DacOutput(code), DacOutput(code + 1)) << code;
  }
}

TEST(OutputPinTest, DacClipsCodesOutsideItsRange) {
  EXPECT_DOUBLE_EQ(DacOutput(-512), DacOutput(-128));
  EXPECT_DOUBLE_EQ(DacOutput(511), DacOutput(127));
}

TEST(OutputPinTest, IoKeepsTheTenBitCodes) {
  EXPECT_EQ(PinSamples(OutputPin::kIo, kSamples), kSamples);
}

TEST(OutputPinTest, CountsTheSamplesTheDacClips) {
  EXPECT_EQ(ClippedSamples(kSamples), 4);
  EXPECT_EQ(ClippedSamples({}), 0);
}

}  // namespace
}  // namespace speak5220::core::synth
