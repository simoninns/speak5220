/*
 * File:        pcm24_test.cpp
 * Module:      audio tests
 * Purpose:     Unit tests of the 24-bit quantisation of project audio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/audio/pcm24.h"

#include <cmath>
#include <limits>
#include <vector>

#include "gtest/gtest.h"

namespace speak5220::core::audio {
namespace {

TEST(Pcm24Test, RoundsToTheNearestCode) {
  EXPECT_EQ(ToPcm24(0.0F), 0);
  EXPECT_EQ(ToPcm24(0.5F), 4194304);
  EXPECT_EQ(ToPcm24(-0.5F), -4194304);
  EXPECT_EQ(ToPcm24(1.4F / 8388608.0F), 1);
  EXPECT_EQ(ToPcm24(1.6F / 8388608.0F), 2);
}

TEST(Pcm24Test, ClipsAtFullScale) {
  EXPECT_EQ(ToPcm24(1.0F), kPcm24Max);
  EXPECT_EQ(ToPcm24(3.0F), kPcm24Max);
  EXPECT_EQ(ToPcm24(-1.0F), kPcm24Min);
  EXPECT_EQ(ToPcm24(-3.0F), kPcm24Min);
}

TEST(Pcm24Test, NonFiniteSamplesBecomeSilence) {
  EXPECT_EQ(ToPcm24(std::numeric_limits<float>::quiet_NaN()), 0);
  EXPECT_EQ(ToPcm24(std::numeric_limits<float>::infinity()), 0);
}

TEST(Pcm24Test, EveryCodeSurvivesARoundTrip) {
  for (const int code : {kPcm24Min, -4194305, -1, 0, 1, 12345, kPcm24Max}) {
    EXPECT_EQ(ToPcm24(FromPcm24(code)), code);
  }
}

TEST(Pcm24Test, QuantisingTwiceChangesNothing) {
  std::vector<float> samples = {0.1F, -0.333F, 0.999999F, 1.5F};
  QuantiseToPcm24(samples);
  const std::vector<float> once = samples;
  QuantiseToPcm24(samples);
  EXPECT_EQ(samples, once);
  EXPECT_EQ(samples[3], FromPcm24(kPcm24Max));
}

}  // namespace
}  // namespace speak5220::core::audio
