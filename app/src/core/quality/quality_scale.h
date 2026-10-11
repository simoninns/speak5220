/*
 * File:        quality_scale.h
 * Module:      quality
 * Purpose:     Places a PESQ score on the phrase's own scale, from PESQ's
 *              floor to a little past its anchor's score, in
 *              plain-language bands
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
enum class QualityBand { kBad, kPoor, kFair, kGood, kLpcOrBetter };

// Where a score sits on its phrase's scale.
struct QualityPlacement {
  // The score relative to the anchor: 0 at PESQ's floor, 1 at the
  // anchor's score, more above it. Not clamped.
  double relative = 0.0;
  // Where the bar shows it: 0 at the left, 1 at the right
  // (QualityScale::PositionOf()).
  double position = 0.0;
  QualityBand band = QualityBand::kBad;
};

// Plan §3.3, "Presenting PESQ to the user": no TMS5220 encoding comes near
// PESQ's 4.5, so a score is shown against the phrase's anchor (the PESQ of
// an unquantised LPC-10 vocoder of the same source, AnchorScore()). The
// relative score is (score - 1) / (anchor - 1); the bands divide it at
// thresholds calibrated on the corpus (HLD §9), the top one starting at
// the anchor. The anchor is a reference, not a ceiling: Optimise often
// beats it, so the bar runs on to kTop, past the anchor. Stateless;
// thread-safe.
class QualityScale {
 public:
  // ITU-T P.862.1: MOS-LQO's lowest value, the bottom of the scale.
  static constexpr double kFloor = 1.0;

  // The relative score at the right end of the bar. Of the corpus's
  // encodings after Optimise, 95% score at most 1.16 and 99% at most
  // 1.36; a score beyond kTop is shown at the end.
  static constexpr double kTop = 1.25;

  // The lowest relative score of each band above kBad: kPoor, kFair,
  // kGood and kLpcOrBetter (the anchor's score).
  static constexpr std::array<double, 4> kBandStarts = {0.40, 0.60, 0.75, 1.00};

  // The placement of `score` given the phrase's `anchor` score. With an
  // anchor at or below the floor there is no scale: the relative score is
  // 1 if the score reaches the anchor, else 0.
  static QualityPlacement Place(double score, double anchor);

  // Where the bar shows a relative score: relative / kTop, clamped to
  // [0, 1].
  static double PositionOf(double relative);

  // The band a relative score falls in.
  static QualityBand BandOf(double relative);

  // The band's label, as the GUI shows it next to the colour.
  static std::string_view Label(QualityBand band);
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_QUALITY_SCALE_H_
