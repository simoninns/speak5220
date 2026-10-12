/*
 * File:        lpc_envelope.cpp
 * Module:      quality
 * Purpose:     LPC analysis and spectral envelopes for the quality
 *              measures: the interface and its SPTK implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/lpc_envelope.h"

#include <SPTK/conversion/filter_coefficients_to_spectrum.h>
#include <SPTK/conversion/spectrum_to_spectrum.h>
#include <SPTK/conversion/waveform_to_autocorrelation.h>
#include <SPTK/math/levinson_durbin_recursion.h>

#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::quality {

StatusOr<LpcFit> SptkLpcAnalysis::Analyse(std::span<const double> frame,
                                          int order) {
  if (order < 1 || frame.size() <= static_cast<std::size_t>(order)) {
    return InvalidArgumentError("the frame is too short for the LPC order");
  }
  const sptk::WaveformToAutocorrelation autocorrelation(
      static_cast<int>(frame.size()), order);
  const sptk::LevinsonDurbinRecursion levinson(order);
  if (!autocorrelation.IsValid() || !levinson.IsValid()) {
    return InternalError("SPTK: cannot set up the LPC analysis");
  }
  const std::vector<double> waveform(frame.begin(), frame.end());
  std::vector<double> r;
  if (!autocorrelation.Run(waveform, &r)) {
    return InternalError("SPTK: cannot compute the autocorrelation");
  }
  std::vector<double> coefficients;
  bool stable = false;
  sptk::LevinsonDurbinRecursion::Buffer buffer;
  if (!levinson.Run(r, &coefficients, &stable, &buffer)) {
    return FailedPreconditionError("the frame is silent");
  }
  // Levinson gives [gain, a1, ..., ap].
  return LpcFit{
      .gain = coefficients.front(),
      .a = std::vector<double>(coefficients.begin() + 1, coefficients.end())};
}

StatusOr<std::vector<double>> SptkLpcAnalysis::EnvelopeDb(const LpcFit& fit,
                                                          int fft_length) {
  const int order = static_cast<int>(fit.a.size());
  // No relative floor: every value of the envelope is kept.
  constexpr double kNoFloorDb = -1.0e300;
  const sptk::FilterCoefficientsToSpectrum to_spectrum(
      0, order, fft_length,
      sptk::SpectrumToSpectrum::kLogAmplitudeSpectrumInDecibels, 0.0,
      kNoFloorDb);
  if (!to_spectrum.IsValid()) {
    return InvalidArgumentError("the FFT length does not suit the LPC order");
  }
  std::vector<double> denominator = {1.0};
  denominator.insert(denominator.end(), fit.a.begin(), fit.a.end());
  std::vector<double> spectrum;
  sptk::FilterCoefficientsToSpectrum::Buffer buffer;
  if (!to_spectrum.Run({fit.gain}, denominator, &spectrum, &buffer)) {
    return InternalError("SPTK: cannot compute the envelope");
  }
  return spectrum;
}

}  // namespace speak5220::core::quality
