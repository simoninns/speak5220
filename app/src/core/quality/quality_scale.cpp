/*
 * File:        quality_scale.cpp
 * Module:      quality
 * Purpose:     Places a PESQ score on the phrase's own scale, from PESQ's
 *              floor to a little past its anchor's score, in
 *              plain-language bands
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/quality_scale.h"

#include <algorithm>
#include <string_view>

namespace speak5220::core::quality {

QualityPlacement QualityScale::Place(double score, double anchor) {
  QualityPlacement p;
  if (anchor <= kFloor) {
    p.relative = score >= anchor ? 1.0 : 0.0;
  } else {
    p.relative = (score - kFloor) / (anchor - kFloor);
  }
  p.position = PositionOf(p.relative);
  p.band = BandOf(p.relative);
  return p;
}

double QualityScale::PositionOf(double relative) {
  return std::clamp(relative / kTop, 0.0, 1.0);
}

QualityBand QualityScale::BandOf(double relative) {
  if (relative >= kBandStarts[3]) {
    return QualityBand::kLpcOrBetter;
  }
  if (relative >= kBandStarts[2]) {
    return QualityBand::kGood;
  }
  if (relative >= kBandStarts[1]) {
    return QualityBand::kFair;
  }
  if (relative >= kBandStarts[0]) {
    return QualityBand::kPoor;
  }
  return QualityBand::kBad;
}

std::string_view QualityScale::Label(QualityBand band) {
  switch (band) {
    case QualityBand::kBad:
      return "Bad";
    case QualityBand::kPoor:
      return "Poor";
    case QualityBand::kFair:
      return "Fair";
    case QualityBand::kGood:
      return "Good";
    case QualityBand::kLpcOrBetter:
      return "As good as LPC or better";
  }
  return "Bad";
}

}  // namespace speak5220::core::quality
