/*
 * File:        encoder_test.cpp
 * Module:      encoder tests
 * Purpose:     Unit tests of the encoder's orchestration with mocked
 *              stages, the faint-opening trim, and determinism
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/encoder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_endings.h"
#include "core/lpc/frame.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/encoder_mocks.h"

namespace speak5220::core::encoder {
namespace {

using ::testing::_;
using ::testing::DoubleNear;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::InSequence;
using ::testing::Return;
using ::testing::StrictMock;

const std::vector<int> kK10 = {20, 15, 8, 8, 8, 8, 8, 4, 4, 4};
const std::vector<int> kK4 = {20, 15, 8, 8};

PlanEntry VoicedEntry() {
  return {.kind = PlanKind::kVoiced, .k = kK10, .pitch = 30};
}
PlanEntry UnvoicedEntry() {
  return {.kind = PlanKind::kUnvoiced, .k = kK4, .pitch = 0};
}

class EncoderTest : public ::testing::Test {
 protected:
  EncoderTest()
      : encoder_(analyser_, pitch_tracker_, voicing_classifier_, repeat_search_,
                 level_setter_) {}

  // A segmentation with two frames, and features with the given levels.
  static Segmentation TwoFrames() {
    return {.x = std::vector<double>(800, 0.1),
            .centres = {222, 422},
            .gate = 0.001,
            .loud = 0.2,
            .onset = 200,
            .offset = 500,
            .c0 = 222};
  }
  static std::vector<FrameFeatures> FeaturesWithLevels(double a, double b) {
    std::vector<FrameFeatures> feats(2);
    feats[0].level = a;
    feats[1].level = b;
    return feats;
  }

  StrictMock<testing::MockFrameAnalyser> analyser_;
  StrictMock<testing::MockPitchTracker> pitch_tracker_;
  StrictMock<testing::MockVoicingClassifier> voicing_classifier_;
  StrictMock<testing::MockRepeatSearch> repeat_search_;
  StrictMock<testing::MockLevelSetter> level_setter_;
  Encoder encoder_;
  const std::vector<double> samples_ = std::vector<double>(800, 0.1);
};

TEST_F(EncoderTest, RunsStagesInOrderWithTheOptions) {
  EncoderOptions options;
  options.target_rms = 30.0;
  const std::vector<PlanEntry> plan = {VoicedEntry(), VoicedEntry()};
  const PitchTrack track{.hops = {0, 40}, .periods = {80.0, 80.0}};
  const std::vector<FrameVoicing> voicing(2, {.voiced = true, .period = 80.0});
  {
    InSequence order;
    EXPECT_CALL(analyser_, Segment(_, Eq(options)))
        .WillOnce(Return(TwoFrames()));
    EXPECT_CALL(pitch_tracker_, Track(_, Eq(options))).WillOnce(Return(track));
    EXPECT_CALL(voicing_classifier_, Classify(_, ElementsAre(222, 422)))
        .WillOnce(Return(voicing));
    EXPECT_CALL(analyser_, Features(_, _, Eq(options)))
        .WillOnce(Return(FeaturesWithLevels(0.1, 0.2)));
    EXPECT_CALL(repeat_search_, Plan(_, Eq(options))).WillOnce(Return(plan));
    EXPECT_CALL(level_setter_,
                ShapeLevels(ElementsAre(0.1, 0.2), _, Eq(options)))
        .WillOnce(Return(std::vector<double>{0.1, 0.2}));
    // in RMS = sqrt((0.01 + 0.04) / 2); gain = 30 / that.
    const double gain = 30.0 / std::sqrt(0.025);
    EXPECT_CALL(level_setter_,
                SetLevels(_, ElementsAre(0.1, 0.2), DoubleNear(gain, 1e-9), _))
        .WillOnce(Return(std::vector<int>{10, 9}));
  }
  const auto result = encoder_.Encode(samples_, 8000, options);
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  EXPECT_EQ(result->frames,
            QuietPauses(AddFades(ToFrames(plan, std::vector<int>{10, 9}))));
  ASSERT_TRUE(result->details.has_value());
  EXPECT_EQ(result->details->energies, (std::vector<int>{10, 9}));
  EXPECT_EQ(result->details->centres, (std::vector<int>{222, 422}));
  EXPECT_EQ(result->details->c0, 222);
}

TEST_F(EncoderTest, RejectsAnotherSampleRateBeforeAnyStage) {
  const auto result = encoder_.Encode(samples_, 44100, {});
  EXPECT_EQ(result.GetStatus().Code(), StatusCode::kInvalidArgument);
}

TEST_F(EncoderTest, RejectsNonFiniteSamples) {
  std::vector<double> samples = samples_;
  samples[10] = std::numeric_limits<double>::infinity();
  EXPECT_EQ(encoder_.Encode(samples, 8000, {}).GetStatus().Code(),
            StatusCode::kInvalidArgument);
}

TEST_F(EncoderTest, RejectsOptionsOutOfRange) {
  EncoderOptions options;
  options.window = 0;
  EXPECT_EQ(encoder_.Encode(samples_, 8000, options).GetStatus().Code(),
            StatusCode::kOutOfRange);
}

TEST_F(EncoderTest, NoSoundGivesALoneStop) {
  EXPECT_CALL(analyser_, Segment(_, _)).WillOnce(Return(std::nullopt));
  const auto result = encoder_.Encode(samples_, 8000, {});
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(result->frames, (std::vector<lpc::Frame>{lpc::Frame::Stop()}));
  EXPECT_FALSE(result->details.has_value());
}

TEST_F(EncoderTest, StagesWorkOnTheSquelchedSource) {
  // Loud, then 60 dB quieter, half a second each.
  std::vector<double> samples(8000, 0.5);
  std::fill(samples.begin() + 4000, samples.end(), 0.0005);
  EncoderOptions options;
  options.squelch_db = 30.0;
  std::vector<double> seen;
  EXPECT_CALL(analyser_, Segment(_, _))
      .WillOnce([&seen](std::span<const double> x,
                        const EncoderOptions&) -> std::optional<Segmentation> {
        seen.assign(x.begin(), x.end());
        return std::nullopt;
      });
  ASSERT_TRUE(encoder_.Encode(samples, 8000, options).IsOk());
  EXPECT_EQ(seen, SquelchedSource(samples, options));
  EXPECT_EQ(seen.back(), 0.0);
}

TEST(SquelchedSourceTest, IsACopyWithoutASquelch) {
  const std::vector<double> samples = {0.5, 0.0005, -0.25};
  EXPECT_EQ(SquelchedSource(samples, {}), samples);
}

TEST_F(EncoderTest, AllSilentPlanGivesALoneStop) {
  EXPECT_CALL(analyser_, Segment(_, _)).WillOnce(Return(TwoFrames()));
  EXPECT_CALL(pitch_tracker_, Track(_, _)).WillOnce(Return(PitchTrack{}));
  EXPECT_CALL(voicing_classifier_, Classify(_, _))
      .WillOnce(Return(std::vector<FrameVoicing>(2)));
  EXPECT_CALL(analyser_, Features(_, _, _))
      .WillOnce(Return(FeaturesWithLevels(0.0, 0.0)));
  EXPECT_CALL(repeat_search_, Plan(_, _))
      .WillOnce(Return(std::vector<PlanEntry>{}));
  const auto result = encoder_.Encode(samples_, 8000, {});
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(result->frames, (std::vector<lpc::Frame>{lpc::Frame::Stop()}));
}

TEST(TrimFaintOpeningTest, SkipsLeadingSilence) {
  std::vector<PlanEntry> plan = {{}, VoicedEntry()};
  const OpeningTrim trim =
      TrimFaintOpening(plan, std::vector<double>{0.0, 1.0});
  EXPECT_EQ(trim.lead, 1);
  EXPECT_EQ(trim.dropped, 0);
}

TEST(TrimFaintOpeningTest, DropsFaintUnvoicedOpeningBeforeAPause) {
  std::vector<PlanEntry> plan = {UnvoicedEntry(), {}, {}, VoicedEntry()};
  // 0.01 is 40 dB below the loudest: faint.
  const OpeningTrim trim =
      TrimFaintOpening(plan, std::vector<double>{0.01, 0.0, 0.0, 1.0});
  EXPECT_EQ(trim.lead, 3);
  EXPECT_EQ(trim.dropped, 3);
}

TEST(TrimFaintOpeningTest, KeepsVoicedOrLoudOpenings) {
  std::vector<PlanEntry> voiced = {VoicedEntry(), {}, VoicedEntry()};
  EXPECT_EQ(TrimFaintOpening(voiced, std::vector<double>{0.01, 0.0, 1.0}).lead,
            0);
  std::vector<PlanEntry> loud = {UnvoicedEntry(), {}, VoicedEntry()};
  EXPECT_EQ(TrimFaintOpening(loud, std::vector<double>{0.1, 0.0, 1.0}).lead, 0);
}

TEST(TrimFaintOpeningTest, KeepsOpeningWithNothingAfterIt) {
  std::vector<PlanEntry> plan = {UnvoicedEntry()};
  EXPECT_EQ(TrimFaintOpening(plan, std::vector<double>{0.01}).lead, 0);
}

TEST(TrimFaintOpeningTest, NewFirstRepeatBecomesAFullFrame) {
  PlanEntry repeat = VoicedEntry();
  repeat.kind = PlanKind::kRepeat;
  std::vector<PlanEntry> plan = {UnvoicedEntry(), {}, repeat, VoicedEntry()};
  const OpeningTrim trim =
      TrimFaintOpening(plan, std::vector<double>{0.01, 0.0, 1.0, 1.0});
  EXPECT_EQ(trim.lead, 2);
  EXPECT_EQ(plan[2].kind, PlanKind::kVoiced);
}

// A vowel-like pulse train with a burst of noise, for the real encoder.
std::vector<double> SyntheticSpeech() {
  std::vector<double> x(8000, 0.0);
  const double theta = 2.0 * std::numbers::pi * 600.0 / 8000.0;
  double y1 = 0.0;
  double y2 = 0.0;
  std::uint32_t state = 1;
  for (int n = 1000; n < 7000; ++n) {
    double drive = 0.0;
    if (n < 4500) {
      drive = (n % 72 == 0) ? 1.0 : 0.0;
    } else {
      state = (state * 1664525U) + 1013904223U;
      drive = (static_cast<double>(state >> 8) / 16777216.0 - 0.5) * 0.3;
    }
    const double y = drive + (1.8 * std::cos(theta) * y1) - (0.81 * y2);
    x[static_cast<std::size_t>(n)] = 0.05 * y;
    y2 = y1;
    y1 = y;
  }
  return x;
}

// TESTING.md: the encoder is deterministic.
TEST(EncoderDeterminismTest, SameInputGivesSameFrames) {
  const std::vector<double> x = SyntheticSpeech();
  const auto first = MakeEncoder()->Encode(x, 8000, {});
  const auto second = MakeEncoder()->Encode(x, 8000, {});
  ASSERT_TRUE(first.IsOk());
  ASSERT_TRUE(second.IsOk());
  EXPECT_EQ(first->frames, second->frames);
  ASSERT_GT(first->frames.size(), 10U);
  EXPECT_EQ(first->frames.back(), lpc::Frame::Stop());
  for (const lpc::Frame& f : first->frames) {
    EXPECT_TRUE(lpc::ValidateFrame(f).IsOk());
  }
}

// Quiet pauses (speak5220 datasheet §12.2) are a pass over the frames the
// encoder would otherwise give: nothing else changes.
TEST(EncoderQuietPausesTest, QuietensThePausesOfTheFramesItWouldGive) {
  // The vowel, a 0.3 s pause, then the vowel again.
  std::vector<double> x = SyntheticSpeech();
  std::vector<double> gap(4500, 0.0);
  std::copy(x.begin() + 1000, x.begin() + 3500, gap.begin());
  x = gap;
  x.insert(x.end(), gap.begin(), gap.begin() + 3500);
  const auto result = MakeEncoder()->Encode(x, 8000, {});
  ASSERT_TRUE(result.IsOk());
  ASSERT_TRUE(result->details.has_value());
  const std::vector<lpc::Frame> plain =
      AddFades(ToFrames(result->details->plan, result->details->energies));
  EXPECT_EQ(result->frames, QuietPauses(plain));
  EXPECT_NE(result->frames, plain);
}

}  // namespace
}  // namespace speak5220::core::encoder
