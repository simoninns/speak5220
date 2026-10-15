/*
 * File:        output_alignment.cpp
 * Module:      quality
 * Purpose:     Lines the chip's output up with the input it was encoded
 *              from, by level envelope then by spectrum, and matches its
 *              level, as the quality measures compare them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/output_alignment.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/fft.h"

namespace speak5220::core::quality {

namespace {

// measures.py's mel bank: 40 triangular bands from 100 to 3800 Hz.
constexpr int kMelBands = 40;
constexpr double kMelLowHz = 100.0;
constexpr double kMelHighHz = 3800.0;
constexpr double kSampleRate = 8000.0;
constexpr int kBins = (kAnalysisWindow / 2) + 1;
// Cepstral coefficients 1-24 are compared.
constexpr int kCepstra = 24;

// The level envelope: 2.5 ms blocks, smoothed over four, floored 50 dB
// below its peak.
constexpr int kEnvelopeHop = 20;
constexpr double kEnvelopeRangeDb = 50.0;

// The spectral refinement: ±25 ms in 0.625 ms steps.
constexpr int kRefineRange = 200;
constexpr int kRefineStep = 5;

// Added before taking logs, as measures.py does.
constexpr double kLogFloor = 1e-12;

std::size_t Index(int i) { return static_cast<std::size_t>(i); }

double Mel(double hz) { return 2595 * std::log10(1 + hz / 700.0); }
double InverseMel(double mel) {
  return 700 * (std::pow(10.0, mel / 2595.0) - 1);
}

// measures.py's mel bank.
const std::vector<double>& MelBank() {
  static const std::vector<double> kBank = MelFilterBank(kMelBands);
  return kBank;
}

const std::vector<double>& Hamming() {
  static const std::vector<double> kWindow =
      dsp::HammingWindow(kAnalysisWindow);
  return kWindow;
}

// The windowed analysis frame `frame` of x.
std::vector<double> WindowedFrame(std::span<const double> x, int frame) {
  std::vector<double> f = CentredFrame(x, frame, kAnalysisWindow, kAnalysisHop);
  const std::vector<double>& w = Hamming();
  for (std::size_t i = 0; i < f.size(); ++i) {
    f[i] *= w[i];
  }
  return f;
}

// Cepstral coefficients 1..kCepstra of a mel log spectrum: the
// unnormalised DCT-II, scipy.fft.dct(norm=None).
std::array<double, kCepstra> Cepstrum(std::span<const double> mel_log) {
  static const std::vector<double> kBasis = [] {
    std::vector<double> b(Index(kCepstra * kMelBands));
    for (int k = 1; k <= kCepstra; ++k) {
      for (int n = 0; n < kMelBands; ++n) {
        b[Index(((k - 1) * kMelBands) + n)] =
            std::cos(std::numbers::pi * k * (2 * n + 1) / (2.0 * kMelBands));
      }
    }
    return b;
  }();
  std::array<double, kCepstra> c{};
  for (int k = 0; k < kCepstra; ++k) {
    double sum = 0.0;
    for (int n = 0; n < kMelBands; ++n) {
      sum += mel_log[Index(n)] * kBasis[Index((k * kMelBands) + n)];
    }
    c[Index(k)] = 2.0 * sum;
  }
  return c;
}

// The input's speech frames: within kSpeechDb of its loudest.
std::vector<bool> SpeechFrames(const std::vector<double>& levels) {
  const double loudest = *std::ranges::max_element(levels);
  std::vector<bool> speech(levels.size());
  for (std::size_t i = 0; i < levels.size(); ++i) {
    speech[i] = levels[i] > loudest - kSpeechDb;
  }
  return speech;
}

// measures.py envelope(): the level of each 2.5 ms block in dB,
// smoothed by a 4-point moving average (numpy.convolve, mode "same") and
// floored kEnvelopeRangeDb below its peak.
std::vector<double> Envelope(std::span<const double> x) {
  const int n = static_cast<int>(x.size()) / kEnvelopeHop;
  std::vector<double> e(Index(n));
  std::array<double, kEnvelopeHop> block{};
  for (int i = 0; i < n; ++i) {
    for (int j = 0; j < kEnvelopeHop; ++j) {
      const double v = x[Index((i * kEnvelopeHop) + j)];
      block[Index(j)] = v * v;
    }
    e[Index(i)] = 10 * std::log10(dsp::Mean(block) + kLogFloor);
  }
  std::vector<double> smooth(Index(n));
  for (int i = 0; i < n; ++i) {
    double sum = 0.0;
    for (int j = i - 2; j <= i + 1; ++j) {
      if (j >= 0 && j < n) {
        sum += e[Index(j)] * 0.25;
      }
    }
    smooth[Index(i)] = sum;
  }
  if (n == 0) {
    return smooth;
  }
  const double floor = *std::ranges::max_element(smooth) - kEnvelopeRangeDb;
  for (double& v : smooth) {
    v = std::max(v, floor);
  }
  return smooth;
}

// numpy.corrcoef(a, y)[0, 1]: NaN when either is constant.
double Correlation(std::span<const double> a, std::span<const double> y) {
  const double mean_a = dsp::Mean(a);
  const double mean_y = dsp::Mean(y);
  double saa = 0.0;
  double syy = 0.0;
  double say = 0.0;
  for (std::size_t i = 0; i < a.size(); ++i) {
    const double da = a[i] - mean_a;
    const double dy = y[i] - mean_y;
    saa += da * da;
    syy += dy * dy;
    say += da * dy;
  }
  // numpy.cov scales by the reciprocal of the degrees of freedom.
  const double scale = 1.0 / static_cast<double>(a.size() - 1);
  const double r =
      ((say * scale) / std::sqrt(saa * scale)) / std::sqrt(syy * scale);
  return std::isnan(r) ? r : std::clamp(r, -1.0, 1.0);
}

}  // namespace

int AnalysisFrameCount(int samples) { return (samples / kAnalysisHop) + 1; }

std::vector<double> MelFilterBank(int bands) {
  // numpy.linspace: start + i * step, the last point exactly the stop.
  const double lo = Mel(kMelLowHz);
  const double hi = Mel(kMelHighHz);
  const double step = (hi - lo) / (bands + 1);
  std::vector<double> edges(Index(bands + 2));
  for (int i = 0; i < bands + 2; ++i) {
    edges[Index(i)] = InverseMel(i == bands + 1 ? hi : i * step + lo);
  }
  std::vector<double> fb(Index(bands * kBins), 0.0);
  for (int band = 0; band < bands; ++band) {
    const double a = edges[Index(band)];
    const double c = edges[Index(band + 1)];
    const double b = edges[Index(band + 2)];
    for (int bin = 0; bin < kBins; ++bin) {
      const double f = bin * kSampleRate / kAnalysisWindow;
      const double w = std::min((f - a) / (c - a), (b - f) / (b - c));
      fb[Index((band * kBins) + bin)] = std::max(w, 0.0);
    }
  }
  return fb;
}

std::vector<double> CentredFrame(std::span<const double> x, int frame, int win,
                                 int hop) {
  std::vector<double> f(Index(win), 0.0);
  const int first = (frame * hop) - (win / 2);
  for (int i = 0; i < win; ++i) {
    const int j = first + i;
    if (j >= 0 && std::cmp_less(j, x.size())) {
      f[Index(i)] = x[Index(j)];
    }
  }
  return f;
}

std::vector<double> FrameLevelsDb(std::span<const double> x) {
  const std::vector<double>& w = Hamming();
  std::vector<double> w2(w.size());
  for (std::size_t i = 0; i < w.size(); ++i) {
    w2[i] = w[i] * w[i];
  }
  const double window_power = dsp::PairwiseSum(w2);
  const int n = AnalysisFrameCount(static_cast<int>(x.size()));
  std::vector<double> levels(Index(n));
  for (int i = 0; i < n; ++i) {
    std::vector<double> f = WindowedFrame(x, i);
    for (double& v : f) {
      v *= v;
    }
    levels[Index(i)] =
        10 * std::log10(dsp::PairwiseSum(f) / window_power + kLogFloor);
  }
  return levels;
}

std::vector<double> MelLogSpectrum(std::span<const double> x, int frame) {
  dsp::PocketFft fft;
  const std::vector<std::complex<double>> bins =
      fft.Forward(WindowedFrame(x, frame));
  std::array<double, kBins> power{};
  for (int k = 0; k < kBins; ++k) {
    // numpy.abs(z) ** 2: the magnitude is a hypot.
    const double m = std::abs(bins[Index(k)]);
    power[Index(k)] = m * m;
  }
  const std::vector<double>& fb = MelBank();
  std::vector<double> mel(Index(kMelBands));
  for (int band = 0; band < kMelBands; ++band) {
    double sum = 0.0;
    for (int k = 0; k < kBins; ++k) {
      sum += power[Index(k)] * fb[Index((band * kBins) + k)];
    }
    mel[Index(band)] = 0.5 * std::log(sum + kLogFloor);
  }
  return mel;
}

int AlignByEnvelope(std::span<const double> input,
                    std::span<const double> output) {
  const std::vector<double> a = Envelope(input);
  const std::vector<double> b = Envelope(output);
  if (a.empty() || b.empty()) {
    return 0;
  }
  const int na = static_cast<int>(a.size());
  const int nb = static_cast<int>(b.size());
  const double floor = *std::ranges::min_element(b);
  const int min_overlap = std::min(na, nb) / 2;
  double best = -std::numeric_limits<double>::infinity();
  int lag = 0;
  std::vector<double> y(a.size());
  for (int shift = -nb + 1; shift < na; ++shift) {
    const int lo = std::max(0, shift);
    const int hi = std::min(na, shift + nb);
    if (hi - lo < min_overlap) {
      continue;
    }
    std::ranges::fill(y, floor);
    for (int i = lo; i < hi; ++i) {
      y[Index(i)] = b[Index(i - shift)];
    }
    const double r = Correlation(a, y);
    if (r > best) {
      best = r;
      lag = shift;
    }
  }
  return lag * kEnvelopeHop;
}

int RefineBySpectrum(std::span<const double> input,
                     std::span<const double> output, int lag) {
  const int n = static_cast<int>(input.size());
  const std::vector<bool> speech = SpeechFrames(FrameLevelsDb(input));
  std::vector<int> frames;
  std::vector<std::array<double, kCepstra>> reference;
  for (int i = 0; std::cmp_less(i, speech.size()); ++i) {
    if (speech[Index(i)]) {
      frames.push_back(i);
      reference.push_back(Cepstrum(MelLogSpectrum(input, i)));
    }
  }
  double best = std::numeric_limits<double>::infinity();
  int at = lag;
  for (int d = -kRefineRange; d <= kRefineRange; d += kRefineStep) {
    const std::vector<double> placed = Place(output, lag + d, n);
    std::vector<double> distances(frames.size());
    for (std::size_t f = 0; f < frames.size(); ++f) {
      const std::array<double, kCepstra> c =
          Cepstrum(MelLogSpectrum(placed, frames[f]));
      std::array<double, kCepstra> squares{};
      for (int k = 0; k < kCepstra; ++k) {
        const double diff = reference[f][Index(k)] - c[Index(k)];
        squares[Index(k)] = diff * diff;
      }
      distances[f] = std::sqrt(dsp::PairwiseSum(squares));
    }
    const double distance = dsp::Mean(distances);
    if (distance < best) {
      best = distance;
      at = lag + d;
    }
  }
  return at;
}

std::vector<double> Place(std::span<const double> output, int lag, int n) {
  std::vector<double> y(Index(n), 0.0);
  const int a = std::max(0, lag);
  const int b = std::min(n, lag + static_cast<int>(output.size()));
  for (int i = a; i < b; ++i) {
    y[Index(i)] = output[Index(i - lag)];
  }
  return y;
}

double LevelMatchGain(std::span<const double> input,
                      std::span<const double> degraded) {
  const std::vector<bool> speech = SpeechFrames(FrameLevelsDb(input));
  std::vector<double> in_squares;
  std::vector<double> out_squares;
  for (int i = 0; std::cmp_less(i, speech.size()); ++i) {
    if (!speech[Index(i)]) {
      continue;
    }
    for (double v : CentredFrame(input, i, kAnalysisWindow, kAnalysisHop)) {
      in_squares.push_back(v * v);
    }
    for (double v : CentredFrame(degraded, i, kAnalysisWindow, kAnalysisHop)) {
      out_squares.push_back(v * v);
    }
  }
  const double pr = dsp::Mean(in_squares);
  const double pd = dsp::Mean(out_squares);
  return pd > 0 ? std::sqrt(pr / pd) : 1.0;
}

std::vector<double> MatchLevel(std::span<const double> input,
                               std::span<const double> degraded) {
  const double gain = LevelMatchGain(input, degraded);
  std::vector<double> out(degraded.begin(), degraded.end());
  for (double& v : out) {
    v *= gain;
  }
  return out;
}

AlignedOutput AlignOutput(std::span<const double> input,
                          std::span<const double> output) {
  AlignedOutput aligned;
  aligned.lag = RefineBySpectrum(input, output, AlignByEnvelope(input, output));
  aligned.degraded = MatchLevel(
      input, Place(output, aligned.lag, static_cast<int>(input.size())));
  return aligned;
}

}  // namespace speak5220::core::quality
