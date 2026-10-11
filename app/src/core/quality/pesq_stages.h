/*
 * File:        pesq_stages.h
 * Module:      quality
 * Purpose:     The stages of ITU-T P.862 PESQ: level alignment, IRS
 *              filtering, voice activity, time alignment and the
 *              perceptual model, for Pesq and for tests
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_PESQ_STAGES_H_
#define SPEAK5220_CORE_QUALITY_PESQ_STAGES_H_

#include <array>
#include <span>
#include <vector>

#include "core/quality/pesq_tables.h"
#include "core/util/status.h"

// The stages are written from the Recommendation's description, with the
// details it leaves open (thresholds, constants, the order of operations)
// as the ANSI-C reference implementation has them, which P.862 Annex A
// gives precedence (plan decision D6). They keep its single-precision
// arithmetic, because a score is the outcome of many threshold decisions.
// All functions are pure apart from their in-out arguments, and are
// thread-safe on distinct arguments.

namespace speak5220::core::quality::pesq {

// One signal as PESQ holds it: kSearchBufferSamples zeros, the samples,
// then kSearchBufferSamples + kDataPaddingSamples zeros.
struct Signal {
  std::vector<float> data;
  // The samples plus both search buffers (the reference's Nsamples); the
  // data padding follows.
  int samples = 0;
  // Per 4 ms envelope frame: the voice-activity level (0 outside speech)
  // and its log relative to the speech threshold.
  std::vector<float> vad;
  std::vector<float> log_vad;
};

// Pads `samples` into a Signal.
Signal MakeSignal(std::span<const float> samples);

// The delay of each utterance found by time alignment (the reference's
// ERROR_INFO). Envelope-frame positions count kDownsample samples; delays
// are in samples, positive when the degraded signal lags.
struct Alignment {
  int utterances = 0;
  int largest_utterance = 0;
  int crude_delay = 0;
  std::array<int, pesq_tables::kMaxUtterances> search_start{};
  std::array<int, pesq_tables::kMaxUtterances> search_end{};
  std::array<int, pesq_tables::kMaxUtterances> delay_estimate{};
  std::array<int, pesq_tables::kMaxUtterances> delay{};
  std::array<float, pesq_tables::kMaxUtterances> delay_confidence{};
  std::array<int, pesq_tables::kMaxUtterances> start{};
  std::array<int, pesq_tables::kMaxUtterances> end{};
};

// Scratch memory the alignment stages share, laid out as the reference
// lays out its temporary buffer: a stage may find what an earlier one left
// there, and the scores depend on it.
using Scratch = std::vector<float>;

// A scratch buffer large enough for two signals of these sizes.
Scratch MakeScratch(const Signal& ref, const Signal& deg);

// ---- Level alignment and filtering (ITU-T P.862 §10.1.1, §10.1.2) ----

// The gain in dB of a piecewise-linear response at `hz`, interpolated
// between its points and extrapolated beyond its ends.
float InterpolateCurve(float hz,
                       std::span<const pesq_tables::CurvePoint> curve);

// Filters the signal part of `data` (from kSearchBufferSamples, for
// `samples` - 2 kSearchBufferSamples + kDataPaddingSamples values) in the
// frequency domain with `curve`, normalised to 0 dB at 1 kHz: one FFT over
// the whole length.
void ApplyFilterCurve(std::span<float> data, int samples,
                      std::span<const pesq_tables::CurvePoint> curve);

// The sum of x[start, stop) squared, divided by `divisor`.
double PowerOf(std::span<const float> x, int start, int stop, int divisor);

// ITU-T P.862 §10.1.1: scales `signal` so its power through the
// level-alignment filter is kTargetAveragePower. `max_samples` is the
// larger of the two signals' samples.
void FixPowerLevel(Signal& signal, int max_samples);

// Removes the mean of the signal part and tapers its first and last 4 ms.
void DcBlock(std::span<float> data, int samples);

// The input filter (kInputFilter) over data[0, length).
void ApplyInputFilter(std::span<float> data, int length);

// ITU-T P.862 §10.1.3.1: the energy of each 4 ms frame, a speech
// threshold found iteratively from the noise floor, and the frames above
// it, with short bursts dropped, short gaps joined and the edges ramped.
// Fills signal.vad and signal.log_vad.
void ComputeVoiceActivity(Signal& signal);

// ---- Time alignment (ITU-T P.862 §10.1.3) ----

// Which span crude alignment works on.
inline constexpr int kWholeSignal = -1;
inline constexpr int kSplitTest = pesq_tables::kMaxUtterances;

// ITU-T P.862 §10.1.3.1: the delay that best correlates the two log
// envelopes, to the nearest 4 ms, for the whole signal (kWholeSignal),
// an utterance, or the span under test in an utterance split
// (kSplitTest).
void CrudeAlign(const Signal& ref, const Signal& deg, Alignment& alignment,
                int utterance, Scratch& scratch);

// ITU-T P.862 §10.1.3.2: the sample-accurate delay of an utterance and
// its confidence, from a histogram of the cross-correlation peaks of 64 ms
// Hann frames.
void TimeAlign(const Signal& ref, const Signal& deg, Alignment& alignment,
               int utterance, Scratch& scratch);

// ITU-T P.862 §10.1.3.3: finds the utterances, aligns each, then splits
// any utterance whose two parts align better apart.
Status LocateUtterances(const Signal& ref, const Signal& deg,
                        Alignment& alignment, Scratch& scratch);

// ---- The perceptual model (ITU-T P.862 §10.2) ----

// ITU-T P.862 §10.2.5: sums the power spectrum's bins into the Bark bands,
// normalised and scaled by kPowerScale.
void FrequencyWarp(std::span<const float> hz_spectrum,
                   std::span<float> pitch_power_density);

// The sum of the bands (from band 1) whose density exceeds `factor` times
// the hearing threshold.
float TotalAudible(std::span<const float> pitch_power_density, float factor);

// ITU-T P.862 §10.2.8: Zwicker's law, density to loudness (Sone), with
// the power raised below 4 Bark.
void IntensityWarp(std::span<const float> pitch_power_density,
                   std::span<float> loudness_density);

// ITU-T P.862 §10.2.11: the width-weighted Lp norm over bands 1 upwards.
float PseudoLp(std::span<const float> x, float p);

// ITU-T P.862 §10.2.10: multiplies each band of `disturbance` by the
// asymmetry factor of the two densities: their ratio to the power 1.2,
// zero below 3 and clipped at 12.
void MultiplyByAsymmetry(std::span<float> disturbance,
                         std::span<const float> ref_density,
                         std::span<const float> deg_density);

// ITU-T P.862 §10.2.14-10.2.15: frame disturbances aggregated with an Lp
// norm over split-second intervals of 20 frames (50% overlapped), then
// with an Lq norm over time, each interval weighted by `time_weight`.
float LpqWeight(int start_frame, int stop_frame, float power_syllable,
                float power_time, std::span<const float> frame_disturbance,
                std::span<const float> time_weight);

// ITU-T P.862 §10.2.13: the delay within ±search_range that maximises
// the cross-correlation of the two signals' magnitudes over [start, stop),
// and that maximum, normalised.
struct DelayEstimate {
  int delay = 0;
  float correlation = 0.0F;
};
DelayEstimate ComputeDelay(int start, int stop, int search_range,
                           std::span<const float> x1,
                           std::span<const float> x2);

// ITU-T P.862 §10.2: the raw PESQ score of two aligned, filtered signals
// of equal length.
StatusOr<float> PerceptualModel(Signal& ref, Signal& deg,
                                const Alignment& alignment);

}  // namespace speak5220::core::quality::pesq

#endif  // SPEAK5220_CORE_QUALITY_PESQ_STAGES_H_
