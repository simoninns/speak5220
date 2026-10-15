/*
 * File:        fft.h
 * Module:      dsp
 * Purpose:     Real FFTs: the interface and its pocketfft implementation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_FFT_H_
#define SPEAK5220_CORE_DSP_FFT_H_

#include <complex>
#include <cstddef>
#include <span>
#include <vector>

namespace speak5220::core::dsp {

// The real FFTs the encoder and the quality measures use, with NumPy's
// conventions:
// forward unscaled, inverse scaled by 1/n. Implementations are
// thread-safe.
class IFft {
 public:
  virtual ~IFft() = default;

  // The n/2 + 1 bins of the real input's spectrum (numpy.fft.rfft).
  virtual std::vector<std::complex<double>> Forward(
      std::span<const double> input) = 0;
  // The n real samples whose spectrum is `bins` (numpy.fft.irfft); `bins`
  // holds n/2 + 1 values.
  virtual std::vector<double> Inverse(
      std::span<const std::complex<double>> bins, std::size_t n) = 0;
};

// IFft over pocketfft, the FFT NumPy itself uses, which helps the encoder
// match the Python golden data. Single-threaded per call.
// Stateless; thread-safe.
class PocketFft : public IFft {
 public:
  std::vector<std::complex<double>> Forward(
      std::span<const double> input) override;
  std::vector<double> Inverse(std::span<const std::complex<double>> bins,
                              std::size_t n) override;
};

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_FFT_H_
