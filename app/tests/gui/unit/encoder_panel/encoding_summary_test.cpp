/*
 * File:        encoding_summary_test.cpp
 * Module:      encoder panel tests
 * Purpose:     Unit tests of the encoder panel's text: statistics, levels,
 *              the quality line and changed options
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/encoding_summary.h"

#include <QString>
#include <optional>

#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/lpc/frame_statistics.h"
#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

core::lpc::FrameStatistics Stats() {
  core::lpc::FrameStatistics stats;
  stats.frames = 40;
  stats.voiced = 20;
  stats.unvoiced = 8;
  stats.repeat = 8;
  stats.silence = 4;
  stats.bits = 1248;
  stats.bytes = 156;
  return stats;
}

TEST(EncodingSummaryTest, FramesTextGivesFramesBitsBytesAndRate) {
  EXPECT_EQ(FramesText(Stats()),
            QStringLiteral("40 frames, 1248 bits (156 B), 1248 bit/s"));
}

TEST(EncodingSummaryTest, KindsTextGivesEachKindsShare) {
  EXPECT_EQ(KindsText(Stats()),
            QStringLiteral("voiced 50% · unvoiced 20% · repeat 20% · "
                           "silence 10%"));
  EXPECT_TRUE(KindsText({}).isEmpty());
}

TEST(EncodingSummaryTest, LevelTextGivesWhatIsKnown) {
  EXPECT_TRUE(LevelText(std::nullopt, std::nullopt).isEmpty());
  EXPECT_EQ(LevelText(112, std::nullopt),
            QStringLiteral("peak code 112 of 127"));
  EXPECT_EQ(LevelText(std::nullopt,
                      core::encoder::LevelStats{
                          .silenced = 0, .limited = 0, .sounding = 30}),
            QStringLiteral("limiter idle"));
  EXPECT_EQ(LevelText(120,
                      core::encoder::LevelStats{
                          .silenced = 1, .limited = 3, .sounding = 30}),
            QStringLiteral("peak code 120 of 127 · limiter lowered 3 frames, "
                           "silenced 1"));
}

TEST(EncodingSummaryTest, QualityTextGivesTheScoreTheAnchorAndTheBand) {
  EXPECT_EQ(QualityText(2.08, 2.31),
            QStringLiteral("PESQ 2.08 (LPC reference 2.31): Good"));
  EXPECT_EQ(QualityText(2.08, std::nullopt), QStringLiteral("PESQ 2.08"));
  EXPECT_EQ(QualityText(std::nullopt, 2.31),
            QStringLiteral("Quality not measured"));
}

TEST(EncodingSummaryTest, ScoreAtOrAboveTheAnchorIsLpcOrBetter) {
  EXPECT_TRUE(QualityText(2.31, 2.31)
                  .endsWith(QStringLiteral(": As good as LPC or better")));
  EXPECT_TRUE(QualityText(2.30, 2.31).endsWith(QStringLiteral(": Good")));
  EXPECT_TRUE(QualityText(2.6, 2.31).endsWith(
      QStringLiteral(": As good as LPC or better")));
}

TEST(EncodingSummaryTest, ToolTipExplainsPesqAndItsLimits) {
  const QString tip = QualityToolTip();
  EXPECT_TRUE(tip.contains(QStringLiteral("4.5")));
  EXPECT_TRUE(tip.contains(QStringLiteral("LPC-10")));
  EXPECT_TRUE(tip.contains(QStringLiteral("not validated for vocoders")));
}

TEST(EncodingSummaryTest, ChangesTextNamesTheOptionsThatDiffer) {
  const core::encoder::EncoderOptions from;
  core::encoder::EncoderOptions to = from;
  EXPECT_EQ(ChangesText(from, to), QStringLiteral("no change"));
  to.target_rms = 22.0;
  to.pre_emphasis = core::encoder::PreEmphasis::FirstOrder(0.9);
  EXPECT_EQ(ChangesText(from, to),
            QStringLiteral("Target RMS 22, Pre-emphasis 0.9"));
}

}  // namespace
}  // namespace speak5220::app
