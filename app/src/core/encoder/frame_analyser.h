/*
 * File:        frame_analyser.h
 * Module:      encoder
 * Purpose:     Finds the speech, places the frames and measures each
 *              frame's level and spectrum
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_FRAME_ANALYSER_H_
#define SPEAK5220_CORE_ENCODER_FRAME_ANALYSER_H_

#include <optional>
#include <span>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"

namespace speak5220::core::encoder {

// Where the speech is, and where its frames are.
struct Segmentation {
  // The input after the 60 Hz high-pass.
  std::vector<double> x;
  // Frame centres, in input samples, every 200 samples from c0.
  std::vector<int> centres;
  // The silence gate and the loudest frame level.
  double gate = 0.0;
  double loud = 0.0;
  // The first and last 2.5 ms block above the gate, in input samples.
  int onset = 0;
  int offset = 0;
  // The first frame's centre: the sound starts at output sample 203
  // (speak5220 datasheet §8.5), 22 samples before kReachedSample.
  int c0 = 0;
};

// The analysis before quantisation. Implementations are stateless.
class IFrameAnalyser {
 public:
  virtual ~IFrameAnalyser() = default;

  // High-passes `samples` (8 kHz) and finds the speech in it: nullopt when
  // nothing rises above the gate.
  virtual std::optional<Segmentation> Segment(
      std::span<const double> samples, const EncoderOptions& options) = 0;

  // Each frame's level and spectra, with its voicing (one per centre).
  virtual std::vector<FrameFeatures> Features(
      const Segmentation& segmentation, std::span<const FrameVoicing> voicing,
      const EncoderOptions& options) = 0;
};

// The Hann-weighted RMS of `x` over 2 half + 1 samples around `centre`
// (25 ms by default), with samples outside `x` left out of the sum but not
// of the weight. Re-implements tms5220enc.py _frameRms(). O(half).
double FrameRms(std::span<const double> x, int centre, int half = 100);

// IFrameAnalyser as tms5220enc.py analyse():
// - a level envelope (10 ms blocks every 2.5 ms) finds the onset and
//   offset, against a gate gate_db below the loudest frame (or twice the
//   envelope's 5th percentile, if higher);
// - frames are 200 samples apart from c0;
// - each frame's spectrum is the lag-windowed autocorrelation of a Hamming
//   window, of the pre-emphasised signal for voiced frames and the plain
//   one for unvoiced; the first frame's window starts at the onset.
// Stateless; thread-safe.
class FrameAnalyser : public IFrameAnalyser {
 public:
  std::optional<Segmentation> Segment(std::span<const double> samples,
                                      const EncoderOptions& options) override;
  std::vector<FrameFeatures> Features(const Segmentation& segmentation,
                                      std::span<const FrameVoicing> voicing,
                                      const EncoderOptions& options) override;
};

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_FRAME_ANALYSER_H_
