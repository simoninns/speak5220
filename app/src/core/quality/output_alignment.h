/*
 * File:        output_alignment.h
 * Module:      quality
 * Purpose:     Lines the chip's output up with the input it was encoded
 *              from, by level envelope then by spectrum, and matches its
 *              level, as the quality measures compare them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_OUTPUT_ALIGNMENT_H_
#define SPEAK5220_CORE_QUALITY_OUTPUT_ALIGNMENT_H_

#include <span>
#include <vector>

namespace speak5220::core::quality {

// The analysis grid of investigation/tools/speechqual.py: 32 ms Hamming
// frames every 10 ms at 8 kHz.
inline constexpr int kAnalysisHop = 80;
inline constexpr int kAnalysisWindow = 256;

// Frames within this many dB of the input's loudest are speech.
inline constexpr double kSpeechDb = 35.0;

// The number of frames speechqual.py frames() gives `samples` samples:
// one centred on each multiple of kAnalysisHop.
int AnalysisFrameCount(int samples);

// Triangular mel filters from 100 to 3800 Hz over the kAnalysisWindow / 2
// + 1 bins of an 8 kHz real FFT: `bands` rows, as speechqual.py and
// evaluate.py melBank() build them.
std::vector<double> MelFilterBank(int bands);

// frame i of x, centred on sample i * hop, zero-padded at the ends, as
// speechqual.py frames().
std::vector<double> CentredFrame(std::span<const double> x, int frame, int win,
                                 int hop);

// Each analysis frame's level in dB (Hamming window, mean square),
// speechqual.py levelDb(). O(n).
std::vector<double> FrameLevelsDb(std::span<const double> x);

// The 40 mel bands' log magnitudes (natural log) of analysis frame
// `frame`, speechqual.py melLog(). O(window log window).
std::vector<double> MelLogSpectrum(std::span<const double> x, int frame);

// The samples by which `output` must be delayed to line up with `input`:
// the shift, in 2.5 ms steps, that best correlates their level envelopes
// (speechqual.py align()). O(n^2) in envelope steps.
int AlignByEnvelope(std::span<const double> input,
                    std::span<const double> output);

// The lag within 25 ms of `lag`, in 0.625 ms steps, whose placed output
// has the lowest mean mel-cepstral distance from the input over its speech
// frames (speechqual.py refine()).
int RefineBySpectrum(std::span<const double> input,
                     std::span<const double> output, int lag);

// `output` delayed by `lag` samples, cut or zero-padded to `n` samples
// (speechqual.py placed()).
std::vector<double> Place(std::span<const double> output, int lag, int n);

// The gain that gives `degraded` the input's mean power over the input's
// speech frames (the scale of speechqual.py matchLevel()); 1 if `degraded`
// is silent there.
double LevelMatchGain(std::span<const double> input,
                      std::span<const double> degraded);

// `degraded` scaled to the input's mean power over the input's speech
// frames (speechqual.py matchLevel()); unchanged if it is silent there.
std::vector<double> MatchLevel(std::span<const double> input,
                               std::span<const double> degraded);

// The chip's output lined up with the input and level-matched, as
// speechqual.py measure() prepared it.
struct AlignedOutput {
  std::vector<double> degraded;
  int lag = 0;
};
AlignedOutput AlignOutput(std::span<const double> input,
                          std::span<const double> output);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_OUTPUT_ALIGNMENT_H_
