/*
 * File:        estoi.cpp
 * Module:      quality
 * Purpose:     The intelligibility measure: extended short-time objective
 *              intelligibility (ESTOI), a port of pystoi's
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/estoi.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <numbers>
#include <numeric>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/fft.h"
#include "core/util/logging.h"

namespace speak5220::core::quality {
namespace {

// pystoi's EPS, numpy.finfo("float").eps.
constexpr double kEps = std::numeric_limits<double>::epsilon();
// The rate of the signals the measures prepare.
constexpr int kInputRate = 8000;

// The modified Bessel function I0, by its power series, which converges
// quickly for the Kaiser window's arguments (at most beta, here 5.65).
double BesselI0(double x) {
  double sum = 1.0;
  double term = 1.0;
  const double quarter_x2 = x * x / 4.0;
  for (int k = 1; k < 64; ++k) {
    term *= quarter_x2 / (static_cast<double>(k) * k);
    sum += term;
    if (term < sum * kEps) {
      break;
    }
  }
  return sum;
}

// numpy.sinc().
double Sinc(double x) {
  if (x == 0.0) {
    return 1.0;
  }
  const double px = std::numbers::pi * x;
  return std::sin(px) / px;
}

// pystoi's window, numpy.hanning(kEstoiFrame + 2)[1:-1]: MATLAB's
// hanning(kEstoiFrame).
const std::array<double, kEstoiFrame>& HannWindow() {
  static const std::array<double, kEstoiFrame> kWindow = [] {
    std::array<double, kEstoiFrame> w{};
    for (int n = 0; n < kEstoiFrame; ++n) {
      w[static_cast<std::size_t>(n)] =
          0.5 -
          0.5 * std::cos(2.0 * std::numbers::pi * (n + 1) / (kEstoiFrame + 1));
    }
    return w;
  }();
  return kWindow;
}

// The starts of pystoi's frames of an n-sample signal:
// range(0, n - kEstoiFrame, kEstoiHop), which leaves out a last frame that
// would end exactly at n.
std::vector<std::size_t> FrameStarts(std::size_t n) {
  std::vector<std::size_t> starts;
  for (std::size_t i = 0; i + kEstoiFrame < n; i += kEstoiHop) {
    starts.push_back(i);
  }
  return starts;
}

// Subtracts each row's mean and divides it by its norm, a row with no
// variation becoming zero (pystoi row_col_normalize(), its first half).
void NormaliseRows(std::vector<std::vector<double>>& m) {
  for (std::vector<double>& row : m) {
    const double mean = std::accumulate(row.begin(), row.end(), 0.0) /
                        static_cast<double>(row.size());
    double sum_squares = 0.0;
    for (double& v : row) {
      v -= mean;
      sum_squares += v * v;
    }
    const double scale = sum_squares > 0.0 ? 1.0 / std::sqrt(sum_squares) : 0.0;
    for (double& v : row) {
      v *= scale;
    }
  }
}

// The same for each column (its second half).
void NormaliseColumns(std::vector<std::vector<double>>& m) {
  const std::size_t columns = m.front().size();
  for (std::size_t c = 0; c < columns; ++c) {
    double mean = 0.0;
    for (const std::vector<double>& row : m) {
      mean += row[c];
    }
    mean /= static_cast<double>(m.size());
    double sum_squares = 0.0;
    for (std::vector<double>& row : m) {
      row[c] -= mean;
      sum_squares += row[c] * row[c];
    }
    const double scale = sum_squares > 0.0 ? 1.0 / std::sqrt(sum_squares) : 0.0;
    for (std::vector<double>& row : m) {
      row[c] *= scale;
    }
  }
}

}  // namespace

std::array<ThirdOctaveBand, kEstoiBands> ThirdOctaveBands() {
  constexpr int kBins = kEstoiFft / 2 + 1;
  // The nearest bin, the first on a tie (numpy.argmin()).
  const auto nearest = [](double hz) {
    int best = 0;
    double best_error = std::numeric_limits<double>::infinity();
    for (int k = 0; k < kBins; ++k) {
      const double f = static_cast<double>(k) * kEstoiRate / kEstoiFft;
      const double error = (f - hz) * (f - hz);
      if (error < best_error) {
        best_error = error;
        best = k;
      }
    }
    return best;
  };
  std::array<ThirdOctaveBand, kEstoiBands> bands{};
  for (int k = 0; k < kEstoiBands; ++k) {
    const double low =
        kEstoiLowestCentreHz * std::pow(2.0, (2.0 * k - 1.0) / 6.0);
    const double high =
        kEstoiLowestCentreHz * std::pow(2.0, (2.0 * k + 1.0) / 6.0);
    bands[static_cast<std::size_t>(k)] = {.first = nearest(low),
                                          .end = nearest(high)};
  }
  return bands;
}

std::vector<double> ResampleLikeOctave(std::span<const double> x, int from_rate,
                                       int to_rate) {
  const int divisor = std::gcd(from_rate, to_rate);
  const int up = to_rate / divisor;
  const int down = from_rate / divisor;
  if (up == down) {
    return {x.begin(), x.end()};
  }
  // pystoi _resample_window_oct(): Octave's resample() filter.
  constexpr double kRejectionDb = 60.0;
  const double cutoff = 1.0 / (2.0 * std::max(up, down));
  const double roll_off = cutoff / 10.0;
  const auto half =
      static_cast<int>(std::ceil((kRejectionDb - 8.0) / (28.714 * roll_off)));
  const double beta = 0.1102 * (kRejectionDb - 8.7);
  const int taps = 2 * half + 1;
  std::vector<double> h(static_cast<std::size_t>(taps));
  const double i0_beta = BesselI0(beta);
  for (int n = 0; n < taps; ++n) {
    // numpy.kaiser(taps, beta) times the ideal filter.
    const double r = 2.0 * n / (taps - 1) - 1.0;
    const double kaiser = BesselI0(beta * std::sqrt(1.0 - r * r)) / i0_beta;
    const double ideal = 2.0 * up * cutoff * Sinc(2.0 * cutoff * (n - half));
    h[static_cast<std::size_t>(n)] = kaiser * ideal;
  }
  // resample_oct() normalises the filter to unit sum; resample_poly()
  // scales it by `up`.
  const double sum = std::accumulate(h.begin(), h.end(), 0.0);
  for (double& v : h) {
    v = v / sum * up;
  }

  // scipy.signal.resample_poly(): the filter delayed so that the output
  // samples fall at its centre, then upfirdn() and the middle n_out
  // samples kept.
  const int pre_pad = down - half % down;
  const int pre_remove = (half + pre_pad) / down;
  const auto n_in = static_cast<std::int64_t>(x.size());
  const std::int64_t n_out = (n_in * up + down - 1) / down;
  std::vector<double> y(static_cast<std::size_t>(n_out), 0.0);
  for (std::int64_t k = 0; k < n_out; ++k) {
    // Output sample k + pre_remove of upfirdn(): the sum over the input
    // samples m of filter tap (k + pre_remove) x down - m x up - pre_pad.
    const std::int64_t base = (k + pre_remove) * down - pre_pad;
    const std::int64_t m_low =
        std::max<std::int64_t>(0, (base - (taps - 1) + up - 1) / up);
    const std::int64_t m_high = std::min(n_in - 1, base >= 0 ? base / up : -1);
    double acc = 0.0;
    for (std::int64_t m = m_low; m <= m_high; ++m) {
      acc += h[static_cast<std::size_t>(base - m * up)] *
             x[static_cast<std::size_t>(m)];
    }
    y[static_cast<std::size_t>(k)] = acc;
  }
  return y;
}

std::pair<std::vector<double>, std::vector<double>> RemoveSilentFrames(
    std::span<const double> x, std::span<const double> y) {
  const std::array<double, kEstoiFrame>& w = HannWindow();
  const std::vector<std::size_t> starts = FrameStarts(x.size());
  std::vector<double> energies;
  energies.reserve(starts.size());
  for (const std::size_t start : starts) {
    double sum_squares = 0.0;
    for (std::size_t i = 0; i < kEstoiFrame; ++i) {
      const double v = w[i] * x[start + i];
      sum_squares += v * v;
    }
    energies.push_back(20.0 * std::log10(std::sqrt(sum_squares) + kEps));
  }
  std::vector<std::size_t> kept;
  if (!energies.empty()) {
    const double loudest = *std::ranges::max_element(energies);
    for (std::size_t f = 0; f < starts.size(); ++f) {
      if (loudest - kEstoiDynamicRangeDb - energies[f] < 0.0) {
        kept.push_back(starts[f]);
      }
    }
  }
  if (kept.empty()) {
    return {};
  }
  // pystoi _overlap_and_add(): the kept frames, windowed, every kEstoiHop.
  const std::size_t n = (kept.size() - 1) * kEstoiHop + kEstoiFrame;
  std::vector<double> x_kept(n, 0.0);
  std::vector<double> y_kept(n, 0.0);
  for (std::size_t f = 0; f < kept.size(); ++f) {
    for (std::size_t i = 0; i < kEstoiFrame; ++i) {
      x_kept[f * kEstoiHop + i] += w[i] * x[kept[f] + i];
      y_kept[f * kEstoiHop + i] += w[i] * y[kept[f] + i];
    }
  }
  return {std::move(x_kept), std::move(y_kept)};
}

std::vector<std::vector<double>> BandEnvelopes(std::span<const double> x) {
  const std::array<double, kEstoiFrame>& w = HannWindow();
  const std::array<ThirdOctaveBand, kEstoiBands> bands = ThirdOctaveBands();
  const std::vector<std::size_t> starts = FrameStarts(x.size());
  std::vector<std::vector<double>> envelopes(
      kEstoiBands, std::vector<double>(starts.size(), 0.0));
  dsp::PocketFft fft;
  std::vector<double> frame(kEstoiFft, 0.0);
  for (std::size_t f = 0; f < starts.size(); ++f) {
    for (std::size_t i = 0; i < kEstoiFrame; ++i) {
      frame[i] = w[i] * x[starts[f] + i];
    }
    const std::vector<std::complex<double>> spectrum = fft.Forward(frame);
    for (std::size_t b = 0; b < bands.size(); ++b) {
      double power = 0.0;
      for (int k = bands[b].first; k < bands[b].end; ++k) {
        power += std::norm(spectrum[static_cast<std::size_t>(k)]);
      }
      envelopes[b][f] = std::sqrt(power);
    }
  }
  return envelopes;
}

double SegmentIntelligibility(const std::vector<std::vector<double>>& x,
                              const std::vector<std::vector<double>>& y,
                              int start) {
  const auto segment = [start](const std::vector<std::vector<double>>& m) {
    std::vector<std::vector<double>> s;
    s.reserve(m.size());
    for (const std::vector<double>& row : m) {
      s.emplace_back(row.begin() + start, row.begin() + start + kEstoiSegment);
    }
    NormaliseRows(s);
    NormaliseColumns(s);
    return s;
  };
  const std::vector<std::vector<double>> xn = segment(x);
  const std::vector<std::vector<double>> yn = segment(y);
  double sum = 0.0;
  for (std::size_t b = 0; b < xn.size(); ++b) {
    for (std::size_t t = 0; t < kEstoiSegment; ++t) {
      sum += xn[b][t] * yn[b][t];
    }
  }
  return sum / kEstoiSegment;
}

StatusOr<double> Estoi(std::span<const double> x, std::span<const double> y) {
  if (x.size() != y.size()) {
    return InvalidArgumentError("ESTOI needs signals of the same length");
  }
  const std::vector<double> x10 = ResampleLikeOctave(x, kInputRate, kEstoiRate);
  const std::vector<double> y10 = ResampleLikeOctave(y, kInputRate, kEstoiRate);
  const auto [x_kept, y_kept] = RemoveSilentFrames(x10, y10);
  const std::vector<std::vector<double>> x_bands = BandEnvelopes(x_kept);
  const std::vector<std::vector<double>> y_bands = BandEnvelopes(y_kept);
  const auto frames = static_cast<int>(x_bands.front().size());
  if (frames < kEstoiSegment) {
    return FailedPreconditionError(
        "too little sound for ESTOI: it needs about 0.4 s");
  }
  double sum = 0.0;
  const int segments = frames - kEstoiSegment + 1;
  for (int s = 0; s < segments; ++s) {
    sum += SegmentIntelligibility(x_bands, y_bands, s);
  }
  return sum / segments;
}

StatusOr<double> MeasureIntelligibility(const PreparedSignals& prepared) {
  std::vector<double> x = prepared.original;
  std::vector<double> y = prepared.output;
  for (std::size_t i = 0; i < x.size(); ++i) {
    const std::size_t f = i / kVoiceActivityFrame;
    if (f >= prepared.speech.size() || !prepared.speech[f]) {
      x[i] = 0.0;
      y[i] = 0.0;
    }
  }
  StatusOr<double> estoi = Estoi(x, y);
  if (estoi.IsOk()) {
    Logger(kLogModuleQuality)->debug("intelligibility: ESTOI {:.3f}", *estoi);
  }
  return estoi;
}

}  // namespace speak5220::core::quality
