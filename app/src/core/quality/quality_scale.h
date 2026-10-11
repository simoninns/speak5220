/*
 * File:        quality_scale.h
 * Module:      quality
 * Purpose:     Places a PESQ score on the phrase's own scale, from PESQ's
 *              floor to its anchor's score, in plain-language bands
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_QUALITY_SCALE_H_
#define SPEAK5220_CORE_QUALITY_QUALITY_SCALE_H_

#include <array>
#include <string_view>

namespace speak5220::core::quality {

// The bands of the quality scale, worst first.
enum class QualityBand { kBad, kPoor, kFair, kGood, kAsGoodAsLpc };

// Where a score sits on its phrase's scale.
struct QualityPlacement {
  // 0 at PESQ's floor, 1 at the anchor's score or above.
  double position = 0.0;
  QualityBand band = QualityBand::kBad;
};

// Plan §3.3, "Presenting PESQ to the user": no TMS5220 encoding comes near
// PESQ's 4.5, so a score is shown against the phrase's anchor (the PESQ of
// an unquantised LPC-10 vocoder of the same source, AnchorScore()). The
// position is (score - 1) / (anchor - 1), clamped to [0, 1]; the bands
// divide it at thresholds calibrated on the corpus (HLD §9). Stateless;
// thread-safe.
class QualityScale {
 public:
  // ITU-T P.862.1: MOS-LQO's lowest value, the bottom of the scale.
  static constexpr double kFloor = 1.0;

  // The lowest position of each band above kBad: kPoor, kFair, kGood and
  // kAsGoodAsLpc.
  static constexpr std::array<double, 4> kBandStarts = {0.40, 0.60, 0.75, 0.90};

  // The placement of `score` given the phrase's `anchor` score. A score
  // at or above the anchor is at the top. With an anchor at or below the
  // floor there is no scale: the position is 1 if the score reaches the
  // anchor, else 0.
  static QualityPlacement Place(double score, double anchor);

  // The band a position (0-1) falls in.
  static QualityBand BandOf(double position);

  // The band's label, as the GUI shows it next to the colour.
  static std::string_view Label(QualityBand band);
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_QUALITY_SCALE_H_
