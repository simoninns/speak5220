/*
 * File:        quality_scale.cpp
 * Module:      quality
 * Purpose:     Places a PESQ score on the phrase's own scale, from PESQ's
 *              floor to its anchor's score, in plain-language bands
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
    p.position = score >= anchor ? 1.0 : 0.0;
  } else {
    p.position = std::clamp((score - kFloor) / (anchor - kFloor), 0.0, 1.0);
  }
  p.band = BandOf(p.position);
  return p;
}

QualityBand QualityScale::BandOf(double position) {
  if (position >= kBandStarts[3]) {
    return QualityBand::kAsGoodAsLpc;
  }
  if (position >= kBandStarts[2]) {
    return QualityBand::kGood;
  }
  if (position >= kBandStarts[1]) {
    return QualityBand::kFair;
  }
  if (position >= kBandStarts[0]) {
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
    case QualityBand::kAsGoodAsLpc:
      return "As good as LPC can achieve";
  }
  return "Bad";
}

}  // namespace speak5220::core::quality
