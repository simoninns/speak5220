/*
 * File:        encoding_summary.cpp
 * Module:      encoder_panel
 * Purpose:     The encoder panel's text: frame statistics, levels, the
 *              PESQ score against its anchor, and changed options
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/encoding_summary.h"

#include <QString>
#include <QStringList>
#include <cmath>
#include <optional>
#include <string>
#include <string_view>

#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/lpc/frame_statistics.h"
#include "core/quality/quality_scale.h"

namespace speak5220::app {

namespace {

// The DAC's largest code (speak5220 datasheet §8.8).
constexpr int kLargestCode = 127;
constexpr double kPercent = 100.0;

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

QString Score(double value) { return QString::number(value, 'f', 2); }

QString Percent(int part, int whole) {
  return QStringLiteral("%1%").arg(
      std::lround(kPercent * part / static_cast<double>(whole)));
}

}  // namespace

QString FramesText(const core::lpc::FrameStatistics& stats) {
  return QStringLiteral("%1 frames, %2 bits (%3 B), %4 bit/s")
      .arg(stats.frames)
      .arg(stats.bits)
      .arg(stats.bytes)
      .arg(std::lround(stats.BitRate()));
}

QString KindsText(const core::lpc::FrameStatistics& stats) {
  if (stats.frames == 0) {
    return {};
  }
  return QStringLiteral("voiced %1 · unvoiced %2 · repeat %3 · silence %4")
      .arg(Percent(stats.voiced, stats.frames),
           Percent(stats.unvoiced, stats.frames),
           Percent(stats.repeat, stats.frames),
           Percent(stats.silence, stats.frames));
}

QString LevelText(std::optional<int> peak_code,
                  const std::optional<core::encoder::LevelStats>& levels) {
  QStringList parts;
  if (peak_code.has_value()) {
    parts << QStringLiteral("peak code %1 of %2")
                 .arg(*peak_code)
                 .arg(kLargestCode);
  }
  if (levels.has_value()) {
    if (levels->limited == 0 && levels->silenced == 0) {
      parts << QStringLiteral("limiter idle");
    } else {
      parts << QStringLiteral("limiter lowered %1 frames, silenced %2")
                   .arg(levels->limited)
                   .arg(levels->silenced);
    }
  }
  return parts.join(QStringLiteral(" · "));
}

QString QualityText(std::optional<double> pesq, std::optional<double> anchor) {
  if (!pesq.has_value()) {
    return QStringLiteral("Quality not measured");
  }
  if (!anchor.has_value()) {
    return QStringLiteral("PESQ %1").arg(Score(*pesq));
  }
  const core::quality::QualityPlacement placement =
      core::quality::QualityScale::Place(*pesq, *anchor);
  return QStringLiteral("PESQ %1 (LPC best for this phrase %2): %3")
      .arg(Score(*pesq), Score(*anchor),
           ToQString(core::quality::QualityScale::Label(placement.band)));
}

QString QualityToolTip() {
  return QStringLiteral(
      "<p>PESQ (ITU-T P.862, as MOS-LQO) compares the encoded speech with "
      "the original and scores it from 1.0 to 4.5.</p>"
      "<p>No LPC coding comes near 4.5, so the scale runs from PESQ's floor "
      "(1.0) to this phrase's own ceiling: the score of an ideal LPC-10 "
      "vocoder of the same recording, with nothing quantised. An encoding "
      "that reaches it is as good as LPC can achieve.</p>"
      "<p>PESQ was not validated for vocoders, so use it to rank encodings "
      "of a phrase, not to predict what a listener would say.</p>");
}

QString ChangesText(const core::encoder::EncoderOptions& from,
                    const core::encoder::EncoderOptions& to) {
  QStringList parts;
  for (const core::encoder::EncoderOptionInfo& info :
       core::encoder::EncoderOptionInfos()) {
    const std::string after = core::encoder::EncoderOptionText(to, info.field);
    if (after != core::encoder::EncoderOptionText(from, info.field)) {
      parts << QStringLiteral("%1 %2").arg(ToQString(info.label),
                                           ToQString(after));
    }
  }
  return parts.isEmpty() ? QStringLiteral("no change")
                         : parts.join(QStringLiteral(", "));
}

}  // namespace speak5220::app
