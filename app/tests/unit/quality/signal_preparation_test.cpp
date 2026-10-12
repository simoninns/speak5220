/*
 * File:        signal_preparation_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the signal preparation for the quality
 *              measures: placement, band-limit, mask, level and alignment
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/signal_preparation.h"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numbers>
#include <span>
#include <vector>

#include "core/quality/output_alignment.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/quality_mocks.h"
#include "support/synthetic_speech.h"

namespace speak5220::core::quality {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using ::testing::Return;

constexpr double kRate = 8000.0;

std::vector<double> Tone(double hz, double amplitude, std::size_t n) {
  std::vector<double> x(n);
  for (std::size_t i = 0; i < n; ++i) {
    x[i] = amplitude * std::sin(2.0 * std::numbers::pi * hz *
                                static_cast<double>(i) / kRate);
  }
  return x;
}

double Rms(std::span<const double> x) {
  double sum = 0.0;
  for (const double v : x) {
    sum += v * v;
  }
  return std::sqrt(sum / static_cast<double>(x.size()));
}

TEST(SignalPreparationTest, PlaceOutputPutsIdleAtZeroAndShifts) {
  // Codes -1 (idle), 0, 2 and 3, as samples.
  const std::vector<std::int16_t> samples = {-64, 0, 128, 192};
  const double step = 64.0 / 32768.0;
  // Source sample s is samples[s + 1]; sample 3 is past the output.
  EXPECT_THAT(PlaceOutput(samples, 1, 4),
              ElementsAre(step, 3 * step, 4 * step, 0.0));
  // Source sample s is samples[s - 2]: the first two are before it.
  EXPECT_THAT(PlaceOutput(samples, -2, 4), ElementsAre(0.0, 0.0, 0.0, step));
  EXPECT_TRUE(PlaceOutput(samples, 0, 0).empty());
}

TEST(SignalPreparationTest, BandLimitRemovesDcAndKeepsTheSpeechBand) {
  std::vector<double> x = Tone(1000.0, 0.5, 8000);
  for (double& v : x) {
    v += 0.25;
  }
  const std::vector<double> y = BandLimit(x);
  ASSERT_EQ(y.size(), x.size());
  const std::span<const double> middle =
      std::span<const double>(y).subspan(2000, 4000);
  double mean = 0.0;
  for (const double v : middle) {
    mean += v;
  }
  EXPECT_NEAR(mean / 4000.0, 0.0, 1e-4);
  EXPECT_NEAR(Rms(middle), 0.5 / std::sqrt(2.0), 0.005);
}

TEST(SignalPreparationTest, BandLimitCutsAboveTheBand) {
  const std::vector<double> y = BandLimit(Tone(3950.0, 0.5, 8000));
  EXPECT_LT(Rms(std::span<const double>(y).subspan(2000, 4000)), 0.05);
}

TEST(SignalPreparationTest, SpeechMaskDropsFramesFarBelowTheLoudest) {
  // Four frames: loud, 30 dB down, 50 dB down, and loud but not speech.
  std::vector<double> x;
  for (const double amplitude : {0.5, 0.5 * 0.0316, 0.5 * 0.00316, 0.5}) {
    const std::vector<double> frame =
        Tone(1000.0, amplitude, kVoiceActivityFrame);
    x.insert(x.end(), frame.begin(), frame.end());
  }
  testing::MockVoiceActivity vad;
  EXPECT_CALL(vad, SpeechFrames(_))
      .WillOnce(Return(std::vector<bool>{true, true, true, false}));
  const StatusOr<std::vector<bool>> speech = SpeechMask(x, vad);
  ASSERT_TRUE(speech.IsOk()) << speech.GetStatus().ToString();
  EXPECT_THAT(*speech, ElementsAre(true, true, false, false));
}

TEST(SignalPreparationTest, SpeechMaskOfSilenceIsEmpty) {
  const std::vector<double> x(std::size_t{2} * kVoiceActivityFrame, 0.0);
  testing::MockVoiceActivity vad;
  EXPECT_CALL(vad, SpeechFrames(_))
      .WillOnce(Return(std::vector<bool>{true, true}));
  const StatusOr<std::vector<bool>> speech = SpeechMask(x, vad);
  ASSERT_TRUE(speech.IsOk());
  EXPECT_THAT(*speech, ElementsAre(false, false));
}

TEST(SignalPreparationTest, SpeechMaskReturnsTheDetectorsFailure) {
  const std::vector<double> x(kVoiceActivityFrame, 0.1);
  testing::MockVoiceActivity vad;
  EXPECT_CALL(vad, SpeechFrames(_)).WillOnce(Return(InternalError("no")));
  EXPECT_EQ(SpeechMask(x, vad).GetStatus().Code(), StatusCode::kInternal);
}

TEST(SignalPreparationTest, LevelGainMatchesPowerOverTheMaskOnly) {
  // The original is 0.4 in frame 0 and 0.1 in frame 1; the output 0.1 and
  // 0.9. Only frame 0 is speech.
  std::vector<double> original(std::size_t{2} * kVoiceActivityFrame, 0.4);
  std::vector<double> output(std::size_t{2} * kVoiceActivityFrame, 0.1);
  for (int i = kVoiceActivityFrame; i < 2 * kVoiceActivityFrame; ++i) {
    original[i] = 0.1;
    output[i] = 0.9;
  }
  EXPECT_NEAR(MaskedLevelGain(original, output, {true, false}), 4.0, 1e-12);
  EXPECT_EQ(MaskedLevelGain(original, std::vector<double>(original.size()),
                            {true, false}),
            1.0);
}

TEST(SignalPreparationTest, PrepareSignalsLinesUpAndMatchesLevels) {
  // The output is the source at a tenth of its level, idle at code -1,
  // delayed by 3 samples (source sample s is output sample s + 3).
  const std::vector<double> source = Tone(500.0, 0.5, 1600);
  EncodedAudio audio;
  audio.source_offset = 3;
  audio.samples.assign(3, -64);
  for (const double v : source) {
    audio.samples.push_back(
        static_cast<std::int16_t>(std::lround(v * 3276.8) - 64));
  }
  testing::MockVoiceActivity vad;
  EXPECT_CALL(vad, SpeechFrames(_))
      .WillOnce(Return(std::vector<bool>(20, true)));
  const StatusOr<PreparedSignals> prepared = PrepareSignals(source, audio, vad);
  ASSERT_TRUE(prepared.IsOk()) << prepared.GetStatus().ToString();
  ASSERT_EQ(prepared->original.size(), source.size());
  ASSERT_EQ(prepared->output.size(), source.size());
  EXPECT_EQ(prepared->speech.size(), 20U);
  for (std::size_t i = 200; i < 1400; ++i) {
    EXPECT_NEAR(prepared->output[i], prepared->original[i], 0.002) << i;
  }
}

TEST(SignalPreparationTest, AlignmentRecoversAKnownDelay) {
  // A delay on the alignment's 0.625 ms grid is recovered to the sample.
  constexpr int kStart = 1000;
  constexpr int kDelay = 235;
  const std::vector<double> x = testing::SyntheticSpeech();
  std::vector<double> out(x.begin() + kStart, x.end());
  const AlignedOutput aligned = AlignOutput(x, testing::Delayed(out, kDelay));
  EXPECT_EQ(aligned.lag, kStart - kDelay);
}

}  // namespace
}  // namespace speak5220::core::quality
