/*
 * File:        encoding_summary.cpp
 * Module:      encoder_panel
 * Purpose:     The encoder panel's text: frame statistics, levels, the
 *              overall quality, its figures, and changed options
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
#include "core/quality/quality_measures.h"
#include "core/quality/quality_score.h"

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

QString QualityText(std::optional<double> overall) {
  if (!overall.has_value()) {
    return QStringLiteral("Quality not measured");
  }
  return QStringLiteral("%1 · %2").arg(
      ToQString(core::quality::RatingLabel(core::quality::RatingOf(*overall))),
      Score(*overall));
}

QString FiguresText(const core::quality::QualityMeasurement& m) {
  return QStringLiteral(
             "envelope %1 dB, %2% over 4 dB · pitch %3% gross, %4 cents RMS "
             "· voicing %5% · ESTOI %6")
      .arg(QString::number(m.envelope.mean_db, 'f', 2),
           QString::number(std::lround(m.envelope.over_4db)),
           QString::number(m.pitch.gross_error, 'f', 1),
           QString::number(std::lround(m.pitch.f0_rmse_cents)),
           QString::number(m.pitch.voicing_error, 'f', 1),
           m.estoi.has_value() ? Score(*m.estoi) : QStringLiteral("too short"));
}

QString QualityToolTip() {
  return QStringLiteral(
      "<p>The encoded speech is compared with the original on the frames "
      "that hold speech; silence, pauses, breaths and clicks are left "
      "out.</p>"
      "<p><b>Envelope</b>: how far the encoding's spectral envelope is from "
      "the original's, as log spectral distortion; under 2 dB is close, "
      "over 4 dB a clear difference.</p>"
      "<p><b>Pitch</b>: how often the pitch is wrong by more than 20% (gross "
      "errors), how far it is out otherwise, in cents, and how often voiced "
      "and unvoiced sounds are swapped.</p>"
      "<p><b>Intelligibility</b>: ESTOI, from 0 to 1. It needs about 0.4 s "
      "of speech.</p>"
      "<p>Each is scored from 0 to 1, and the overall score is their "
      "geometric mean, so one weak part pulls it down. Poor is below 0.50, "
      "Fair from 0.50 and Good from 0.70; unquantised LPC, the best a "
      "TMS5220-style coding could do, is Good on nearly every phrase.</p>");
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
