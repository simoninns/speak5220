/*
 * File:        lpc_envelope.h
 * Module:      quality
 * Purpose:     LPC analysis and spectral envelopes for the quality
 *              measures: the interface and its SPTK implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_LPC_ENVELOPE_H_
#define SPEAK5220_CORE_QUALITY_LPC_ENVELOPE_H_

#include <span>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::quality {

// An all-pole model of a frame, gain / A(z) with
// A(z) = 1 + a[0] z^-1 + ... + a[p-1] z^-p.
struct LpcFit {
  // The square root of the prediction error's energy.
  double gain = 0.0;
  std::vector<double> a;
};

// Fits all-pole models to frames and gives their spectral envelopes.
// Implementations are thread-safe.
class ILpcAnalysis {
 public:
  virtual ~ILpcAnalysis() = default;

  // The order-`order` fit of `frame` (already windowed) by the
  // autocorrelation method. Fails for a silent frame, or one too short for
  // the order.
  virtual StatusOr<LpcFit> Analyse(std::span<const double> frame,
                                   int order) = 0;

  // The envelope gain / |A| in dB (20 log10) at fft_length / 2 + 1 evenly
  // spaced frequencies from 0 to half the sample rate. `fft_length` is a
  // power of two greater than the order.
  virtual StatusOr<std::vector<double>> EnvelopeDb(const LpcFit& fit,
                                                   int fft_length) = 0;
};

// ILpcAnalysis over SPTK 4: WaveformToAutocorrelation,
// LevinsonDurbinRecursion and FilterCoefficientsToSpectrum (plan §3.3).
// Stateless; thread-safe.
class SptkLpcAnalysis : public ILpcAnalysis {
 public:
  StatusOr<LpcFit> Analyse(std::span<const double> frame, int order) override;
  StatusOr<std::vector<double>> EnvelopeDb(const LpcFit& fit,
                                           int fft_length) override;
};

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_LPC_ENVELOPE_H_
