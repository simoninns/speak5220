/*
 * File:        output_pin_test.cpp
 * Module:      synth tests
 * Purpose:     Unit tests of the chip's two outputs: the SPEAKER pin's DAC
 *              clipping and the I/O pin's unclipped codes
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/output_pin.h"

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
  // give -128's.
  EXPECT_THAT(PinSamples(OutputPin::kSpeaker, kSamples),
              ElementsAre(-128 * kSampleScale, -128 * kSampleScale,
                          -128 * kSampleScale, 0, 127 * kSampleScale,
                          127 * kSampleScale, 127 * kSampleScale));
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
