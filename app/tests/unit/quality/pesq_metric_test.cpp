/*
 * File:        pesq_metric_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of PesqMetric with a mocked synth: cutting the
 *              chip's output, scoring it and reporting failures
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pesq_metric.h"

#include <cmath>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/lpc/frame.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/synth_mocks.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

using ::testing::_;
using ::testing::Return;

// The synthetic speech as chip codes: idle (-1) before and after, the
// samples at about 100 codes of peak in between.
synth::SynthResult SpokenAsCodes(const std::vector<double>& x) {
  synth::SynthResult r;
  r.codes.assign(200, -1);
  for (double v : x) {
    r.codes.push_back(static_cast<int>(std::lround(v * 200.0)));
  }
  r.codes.insert(r.codes.end(), 300, -1);
  return r;
}

encoder::EncodeResult OneFrame() {
  encoder::EncodeResult e;
  e.frames = {lpc::Frame::Voiced(10, 30, {}), lpc::Frame::Stop()};
  return e;
}

TEST(PesqMetricTest, ScoresTheOutputCutToItsSoundAndLinedUp) {
  const std::vector<double> x = testing::SyntheticSpeech();
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, synth::SynthPath::kVsm))
      .WillOnce(Return(SpokenAsCodes(x)));
  PesqMetric metric(synth);
  const auto m = metric.MeasureFrames(x, OneFrame().frames);
  ASSERT_TRUE(m.IsOk()) << m.GetStatus().ToString();
  // Idle codes are cut off and the rest moved up by one, so idle is 0.
  const synth::SynthResult spoken = SpokenAsCodes(x);
  std::size_t first = 0;
  while (spoken.codes[first] == -1) {
    ++first;
  }
  EXPECT_EQ(m->output.front(), spoken.codes[first] + 1.0);
  EXPECT_NE(m->output.back(), 0.0);
  EXPECT_GT(m->score, 3.0);
  EXPECT_EQ(metric.Direction(), MetricDirection::kHigherIsBetter);
}

TEST(PesqMetricTest, MeasureReturnsTheScore) {
  const std::vector<double> x = testing::SyntheticSpeech();
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, _)).WillRepeatedly(Return(SpokenAsCodes(x)));
  PesqMetric metric(synth);
  const auto score = metric.Measure(x, OneFrame());
  const auto m = metric.MeasureFrames(x, OneFrame().frames);
  ASSERT_TRUE(score.IsOk() && m.IsOk());
  EXPECT_EQ(*score, m->score);
}

TEST(PesqMetricTest, SilentOutputIsAnError) {
  testing::MockSynth synth;
  synth::SynthResult idle;
  idle.codes.assign(1000, -1);
  EXPECT_CALL(synth, Synthesize(_, _)).WillOnce(Return(idle));
  PesqMetric metric(synth);
  EXPECT_FALSE(metric.Measure(testing::SyntheticSpeech(), OneFrame()).IsOk());
}

TEST(PesqMetricTest, SynthFailuresArePassedOn) {
  testing::MockSynth synth;
  EXPECT_CALL(synth, Synthesize(_, _))
      .WillOnce(Return(InternalError("emulator failed")));
  PesqMetric metric(synth);
  const auto score = metric.Measure(testing::SyntheticSpeech(), OneFrame());
  ASSERT_FALSE(score.IsOk());
  EXPECT_EQ(score.GetStatus().Code(), StatusCode::kInternal);
}

}  // namespace
}  // namespace speak5220::core::quality
