/*
 * File:        anchor_vocoder.cpp
 * Module:      quality
 * Purpose:     The anchor: an unquantised floating-point LPC-10 vocoder of
 *              a phrase's source, whose PESQ is the ceiling of the quality
 *              scale
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/anchor_vocoder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/iir_filter.h"
#include "core/encoder/encoder_filters.h"
#include "core/quality/numpy_normal.h"
#include "core/quality/output_alignment.h"
#include "core/quality/pesq.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

namespace {

constexpr double kSampleRate = 8000.0;

// TrackAnchorPitch(): a 40 ms correlation window over lags of 20-160
// samples (400 Hz down to 50 Hz).
constexpr int kPitchWindow = 320;
constexpr int kMinLag = 20;
constexpr int kMaxLag = 160;
constexpr int kLags = kMaxLag - kMinLag + 1;
constexpr double kVoicedCorrelation = 0.5;
// A sub-multiple of the best lag is taken within this fraction of its
// correlation.
constexpr double kSubMultipleRatio = 0.9;

// AnalyseAnchor(): 25 ms frames, 30 ms windows.
constexpr int kHop = 200;
constexpr int kWindow = 240;
constexpr double kPreEmphasis = 0.9375;
constexpr double kLagWindowHz = 60.0;
// White-noise correction of the zero lag.
constexpr double kNoiseCorrection = 1.0001;
constexpr double kTiny = 1e-12;

// SynthesiseAnchor(): 8 interpolation steps a frame.
constexpr int kSteps = 8;
constexpr int kSubframe = kHop / kSteps;

std::size_t Index(int i) { return static_cast<std::size_t>(i); }

// A(z) from reflection coefficients (speechqual.py polyOf()).
std::array<double, kAnchorOrder + 1> PolynomialOf(
    const std::array<double, kAnchorOrder>& k) {
  std::array<double, kAnchorOrder + 1> a{};
  a[0] = 1.0;
  for (int m = 0; m < kAnchorOrder; ++m) {
    // a = [a, 0] + k * [0, reversed(a)], over the m + 2 values in use.
    std::array<double, kAnchorOrder + 1> next{};
    for (int i = 0; i <= m + 1; ++i) {
      const double head = i <= m ? a[Index(i)] : 0.0;
      const double tail = i >= 1 ? a[Index(m + 1 - i)] : 0.0;
      next[Index(i)] = head + k[Index(m)] * tail;
    }
    a = next;
  }
  return a;
}

// scipy.signal.lfilter([g], a, x, zi=state) for a 10th-order all-pole
// filter, transposed direct form II in scipy's order of operations: b is
// padded with zeros to the length of a.
void AllPoleFilter(double g, const std::array<double, kAnchorOrder + 1>& a,
                   std::span<const double> x, std::span<double> y,
                   std::array<double, kAnchorOrder>& z) {
  for (std::size_t n = 0; n < x.size(); ++n) {
    const double xn = x[n];
    const double yn = z[0] + g * xn;
    for (int i = 0; i < kAnchorOrder - 1; ++i) {
      z[Index(i)] = z[Index(i + 1)] + xn * 0.0 - yn * a[Index(i + 1)];
    }
    z[kAnchorOrder - 1] = xn * 0.0 - yn * a[kAnchorOrder];
    y[n] = yn;
  }
}

}  // namespace

AnchorPitch TrackAnchorPitch(std::span<const double> x) {
  const std::vector<double> y = encoder::LowPass1kHz(x);
  const int frames = AnalysisFrameCount(static_cast<int>(x.size()));
  constexpr int kSpan = kPitchWindow + kMaxLag;
  AnchorPitch pitch;
  pitch.f0.resize(Index(frames));
  pitch.voiced.resize(Index(frames));
  std::vector<double> products(kPitchWindow);
  std::vector<double> r(kLags);
  for (int frame = 0; frame < frames; ++frame) {
    const std::vector<double> f = CentredFrame(y, frame, kSpan, kAnalysisHop);
    for (int i = 0; i < kPitchWindow; ++i) {
      products[Index(i)] = f[Index(i)] * f[Index(i)];
    }
    const double ea = dsp::PairwiseSum(products);
    for (int l = 0; l < kLags; ++l) {
      const int lag = kMinLag + l;
      for (int i = 0; i < kPitchWindow; ++i) {
        products[Index(i)] = f[Index(i)] * f[Index(lag + i)];
      }
      const double ab = dsp::PairwiseSum(products);
      for (int i = 0; i < kPitchWindow; ++i) {
        products[Index(i)] = f[Index(lag + i)] * f[Index(lag + i)];
      }
      const double bb = dsp::PairwiseSum(products);
      r[Index(l)] = ab / std::sqrt(ea * bb + kTiny);
    }
    int best = static_cast<int>(std::ranges::max_element(r) - r.begin());
    const double peak = r[Index(best)];
    // Prefer the lag at a third, then a half, of the best (the local
    // maximum next to it), if nearly as well correlated: subharmonics.
    for (int d : {3, 2}) {
      const int sub = static_cast<int>(std::nearbyint(
                          static_cast<double>(kMinLag + best) / d)) -
                      kMinLag;
      const bool ok = sub >= 0;
      const int s = ok ? sub : 0;
      int arg = 0;
      double near_max = r[Index(std::clamp(s - 1, 0, kLags - 1))];
      for (int o = 0; o <= 2; ++o) {
        const double v = r[Index(std::clamp(s + o - 1, 0, kLags - 1))];
        if (v > near_max) {
          near_max = v;
          arg = o;
        }
      }
      const int cand = std::clamp(s + arg - 1, 0, kLags - 1);
      if (ok && r[Index(cand)] >= kSubMultipleRatio * peak) {
        best = cand;
      }
    }
    // Parabolic refinement of the lag.
    const int n0 = std::clamp(best, 1, kLags - 2);
    const double y0 = r[Index(n0 - 1)];
    const double y1 = r[Index(n0)];
    const double y2 = r[Index(n0 + 1)];
    const double den = y0 - 2 * y1 + y2;
    const double off = std::fabs(den) > 1e-9 ? 0.5 * (y0 - y2) / den : 0.0;
    const double lag = (kMinLag + n0) + std::clamp(off, -1.0, 1.0);
    pitch.f0[Index(frame)] = kSampleRate / lag;
    pitch.voiced[Index(frame)] = r[Index(best)] >= kVoicedCorrelation;
  }
  return pitch;
}

std::vector<AnchorFrame> AnalyseAnchor(std::span<const double> x) {
  const int n = static_cast<int>(x.size());
  const std::vector<double> w = dsp::HammingWindow(kWindow);
  std::vector<double> w2(w.size());
  for (std::size_t i = 0; i < w.size(); ++i) {
    w2[i] = w[i] * w[i];
  }
  const double window_power = dsp::PairwiseSum(w2);
  // scipy.signal.lfilter([1, -0.9375], [1], x).
  constexpr std::array<double, 2> kPreEmphasisFir = {1.0, -kPreEmphasis};
  const std::vector<double> y = dsp::FirFilter(kPreEmphasisFir, x);
  std::array<double, kAnchorOrder> lag_window{};
  for (int i = 1; i <= kAnchorOrder; ++i) {
    const double t = 2 * std::numbers::pi * kLagWindowHz * i / kSampleRate;
    lag_window[Index(i - 1)] = std::exp(-0.5 * (t * t));
  }
  const AnchorPitch pitch = TrackAnchorPitch(x);
  const std::vector<double> level = FrameLevelsDb(x);
  const double loudest = *std::ranges::max_element(level);
  const int pitch_frames = static_cast<int>(pitch.f0.size());

  std::vector<AnchorFrame> frames(Index((n / kHop) + 1));
  std::vector<double> seg(kWindow);
  for (int j = 0; std::cmp_less(j, frames.size()); ++j) {
    AnchorFrame& fr = frames[Index(j)];
    const int c = (kHop * j) + (kHop / 2);
    // The median pitch of the voiced speech frames around the centre.
    std::vector<double> v;
    for (int m = (c / kAnalysisHop) - 1; m < (c / kAnalysisHop) + 2; ++m) {
      if (m >= 0 && m < pitch_frames && pitch.voiced[Index(m)] &&
          level[Index(m)] > loudest - kSpeechDb) {
        v.push_back(pitch.f0[Index(m)]);
      }
    }
    fr.period = v.size() >= 2 ? kSampleRate / dsp::Median(v) : 0.0;
    // The pre-emphasised window centred on c.
    for (int i = 0; i < kWindow; ++i) {
      const int s = c - (kWindow / 2) + i;
      seg[Index(i)] = (s >= 0 && s < n ? y[Index(s)] : 0.0) * w[Index(i)];
    }
    std::array<double, kAnchorOrder + 1> r{};
    for (int lag = 0; lag <= kAnchorOrder; ++lag) {
      double sum = 0.0;
      for (int i = 0; i < kWindow - lag; ++i) {
        sum += seg[Index(i)] * seg[Index(i + lag)];
      }
      r[Index(lag)] = sum;
    }
    r[0] = r[0] * kNoiseCorrection + kTiny;
    for (int i = 1; i <= kAnchorOrder; ++i) {
      r[Index(i)] *= lag_window[Index(i - 1)];
    }
    // Levinson's recursion.
    std::array<double, kAnchorOrder + 1> a{};
    a[0] = 1.0;
    double e = r[0];
    for (int i = 1; i <= kAnchorOrder; ++i) {
      double dot = 0.0;
      for (int m = 0; m < i; ++m) {
        dot += a[Index(m)] * r[Index(i - m)];
      }
      const double ki = -dot / e;
      const std::array<double, kAnchorOrder + 1> previous = a;
      for (int m = 0; m <= i; ++m) {
        a[Index(m)] = previous[Index(m)] + ki * previous[Index(i - m)];
      }
      fr.k[Index(i - 1)] = ki;
      e *= 1 - ki * ki;
    }
    fr.gain = std::sqrt(e / window_power);
  }
  return frames;
}

std::vector<double> SynthesiseAnchor(std::span<const AnchorFrame> frames, int n,
                                     NumpyNormal& noise) {
  std::vector<double> out(frames.size() * kHop, 0.0);
  std::array<double, kAnchorOrder> state{};
  double next_pulse = 0.0;
  std::array<double, kSubframe> ex{};
  for (std::size_t j = 0; j < frames.size(); ++j) {
    const AnchorFrame& f = frames[j];
    const std::array<double, kAnchorOrder>& k0 = j > 0 ? frames[j - 1].k : f.k;
    const double g0 = j > 0 ? frames[j - 1].gain : 0.0;
    const double p = f.period;
    for (int s = 0; s < kSteps; ++s) {
      const double t = static_cast<double>(s + 1) / kSteps;
      std::array<double, kAnchorOrder> k{};
      for (int i = 0; i < kAnchorOrder; ++i) {
        k[Index(i)] = k0[Index(i)] + (f.k[Index(i)] - k0[Index(i)]) * t;
      }
      const double g = g0 + (f.gain - g0) * t;
      const int n0 = (static_cast<int>(j) * kHop) + (s * kSubframe);
      for (int i = 0; i < kSubframe; ++i) {
        ex[Index(i)] = 0.0;
        if (p != 0.0) {
          if (n0 + i >= next_pulse) {
            ex[Index(i)] = std::sqrt(p);
            next_pulse += p;
          }
        } else {
          ex[Index(i)] = noise.Next();
          next_pulse = n0 + i + 1;
        }
      }
      AllPoleFilter(g, PolynomialOf(k), ex,
                    std::span(out).subspan(Index(n0), kSubframe), state);
    }
  }
  // scipy.signal.lfilter([1], [1, -0.9375], out): de-emphasis.
  double z = 0.0;
  for (double& v : out) {
    const double yn = z + v;
    z = v * 0.0 - yn * -kPreEmphasis;
    v = yn;
  }
  // A frame's values are reached at its centre: drop the half-frame lag.
  std::vector<double> result(Index(n), 0.0);
  for (int i = 0; i < n && Index(i + (kHop / 2)) < out.size(); ++i) {
    result[Index(i)] = out[Index(i + (kHop / 2))];
  }
  return result;
}

std::vector<double> AnchorVocoder(std::span<const double> x) {
  NumpyNormal noise = NumpyNormal::SeedOne();
  return SynthesiseAnchor(AnalyseAnchor(x), static_cast<int>(x.size()), noise);
}

StatusOr<double> AnchorScore(std::span<const double> source) {
  const std::vector<double> ref = encoder::HighPass60Hz(source);
  const std::vector<double> voc = MatchLevel(ref, AnchorVocoder(ref));
  StatusOr<PesqScore> score = ScorePesq(ref, voc);
  if (!score.IsOk()) {
    return score.GetStatus();
  }
  return static_cast<double>(score->mos_lqo);
}

StatusOr<double> AnchorScorer::Score(std::span<const double> source) {
  return AnchorScore(source);
}

}  // namespace speak5220::core::quality
