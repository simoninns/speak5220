/*
 * File:        frame_map.cpp
 * Module:      waveform
 * Purpose:     Which frame the chip is speaking at each sample of the
 *              encoded audio, and how the encoded pane describes it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/frame_map.h"

#include <QString>
#include <QStringList>
#include <cstdint>
#include <optional>
#include <string_view>

#include "core/encoder/encoder_types.h"
#include "core/lpc/frame.h"
#include "core/quality/encoded_audio.h"

namespace speak5220::app {

namespace lpc = core::lpc;

std::int64_t FrameStretchStart(int frame) {
  return core::quality::kFirstFrameStretch +
         (static_cast<std::int64_t>(core::encoder::kFrameSamples) * frame);
}

std::optional<int> FrameAtEncodedSample(std::int64_t sample, int frame_count) {
  const std::int64_t from_first = sample - core::quality::kFirstFrameStretch;
  if (from_first < 0) {
    return std::nullopt;
  }
  const std::int64_t frame = from_first / core::encoder::kFrameSamples;
  if (frame >= frame_count) {
    return std::nullopt;
  }
  return static_cast<int>(frame);
}

QString DescribeFrame(int index, const lpc::Frame& frame) {
  const lpc::FrameType type = lpc::FrameKind(frame);
  const std::string_view kind = lpc::FrameTypeName(type);
  QString text = QStringLiteral("Frame %1: %2")
                     .arg(index)
                     .arg(QString::fromLatin1(
                         kind.data(), static_cast<qsizetype>(kind.size())));
  // A silence or stop frame carries nothing else (datasheet §8.1).
  if (type == lpc::FrameType::kSilence || type == lpc::FrameType::kStop) {
    return text;
  }
  text += QStringLiteral(", energy %1").arg(frame.energy);
  if (type != lpc::FrameType::kUnvoiced) {
    text += QStringLiteral(", pitch %1").arg(frame.pitch);
  }
  const int k_count = lpc::FrameKCount(frame);
  if (k_count > 0) {
    QStringList k;
    for (int i = 0; i < k_count; ++i) {
      k << QString::number(frame.k.at(static_cast<std::size_t>(i)));
    }
    text += QStringLiteral(", K ") + k.join(QLatin1Char(' '));
  }
  return text;
}

}  // namespace speak5220::app
