/*
 * File:        pitch_tracker.cpp
 * Module:      encoder
 * Purpose:     Voicing and pitch period every 5 ms: YIN candidates joined by
 *              a Viterbi search
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/pitch_tracker.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/fft.h"
#include "core/encoder/encoder_filters.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/util/logging.h"

namespace speak5220::core::encoder {
namespace {

// YIN (tms5220enc.py _yin()): a 30 ms window, correlated by 1024-point
// FFTs. The difference at lag t covers [c - lead, c - lead + win + t]:
// centred on c for t = 80, within 5 ms of it for t = 40 to 160.
constexpr std::size_t kYinWindow = 240;
constexpr std::size_t kYinLead = (kYinWindow / 2) + 40;
constexpr std::size_t kYinFft = 1024;
constexpr std::size_t kYinLags = kYinMaxLag + 1;
constexpr std::size_t kYinSegment = kYinWindow + kYinMaxLag;

// Candidates (tms5220enc.py _candidates()).
constexpr int kCandidateMinLag = 15;
constexpr int kCandidateMaxLag = 161;
constexpr std::size_t kMaxCandidates = 5;
constexpr double kParabolaLimit = 0.5;

// Voicing evidence (tms5220enc.py _tiltAndCrossings()).
constexpr int kEvidenceWindow = 240;
constexpr double kEvidencePreEmphasis = 0.99;
constexpr double kSamplesPer10Ms = 80.0;

// trackPitch(): full-band and low-band RMS over centred 30 ms windows.
constexpr std::size_t kRmsHalf = 120;
constexpr std::size_t kRmsWindow = 2 * kRmsHalf;
constexpr double kLoudPercentile = 95.0;
// Hops quieter than this fraction of the loud level do not vote for the
// median period, which is the first dip below kConfidentDip.
constexpr double kMedianQuiet = 0.03;
constexpr double kConfidentDip = 0.15;
constexpr double kDefaultMedian = 80.0;
// Cost weights: CMNDF scale, per-octave jump, voicing switch, prior per
// octave beyond kPriorFree octave from the median, subharmonic penalty.
constexpr double kAlpha = 10.0;
constexpr double kJump = 12.0;
constexpr double kSwitch = 4.0;
constexpr double kPrior = 5.0;
constexpr double kPriorFree = 0.6;
constexpr double kSubharmonic = 4.0;
// A candidate within 8% of 2x or 3x a clearly periodic one is a
// subharmonic; kSubharmonicSlack lets that one be slightly worse.
constexpr double kSubharmonicTolerance = 0.08;
constexpr double kSubharmonicSlack = 0.01;
// Far below the speech: favour unvoiced.
constexpr double kQuietLevel = 0.02;
constexpr double kQuietCost = 4.0;
// The voicing threshold rises by up to kLowPassRise where the sound is loud
// and low-pass (vowels, nasals), and falls by up to kHighPassFall where it
// is mostly above 1 kHz (fricatives).
constexpr double kLowPassRise = 0.35;
constexpr double kLowPassFrom = 0.6;
constexpr double kHighPassFall = 0.15;
constexpr double kHighPassFrom = 0.5;
constexpr double kRatioSpan = 0.3;
constexpr double kLevelFloorDb = 30.0;
constexpr double kLevelSpanDb = 20.0;
constexpr double kMinimumLevel = 1e-6;
// Tilt and zero crossings: about +0.06 for a steep low-pass hop, -0.075 for
// a fricative at 80 crossings per 10 ms, at weight 1.
constexpr double kCrossingsCentre = 30.0;
constexpr double kCrossingsScale = 10.0;
constexpr double kEvidenceScale = 66.7;
// A short unvoiced gap (under kGapHops hops) cannot be used to change
// octave for less than a jump costs.
constexpr int kGapHops = 8;

double Clamp01(double v) { return std::min(1.0, std::max(0.0, v)); }

// Centred RMS over kRmsWindow samples at each hop, from a running sum of
// squares of the zero-padded signal.
std::vector<double> WindowRms(std::span<const double> v,
                              std::span<const int> centres) {
  std::vector<double> cs(v.size() + kRmsWindow + 1, 0.0);
  double sum = 0.0;
  for (std::size_t i = 0; i < v.size() + kRmsWindow; ++i) {
    double sample = 0.0;
    if (i >= kRmsHalf && i - kRmsHalf < v.size()) {
      sample = v[i - kRmsHalf];
    }
    sum += sample * sample;
    cs[i + 1] = sum;
  }
  std::vector<double> rms;
  rms.reserve(centres.size());
  for (const int c : centres) {
    const auto at = static_cast<std::size_t>(c);
    rms.push_back(std::sqrt((cs[at + kRmsWindow] - cs[at]) /
                            static_cast<double>(kRmsWindow)));
  }
  return rms;
}

// A state of the Viterbi search at one hop: a candidate's lag, or 0 for
// unvoiced.
struct HopStates {
  std::vector<double> lags;
  std::vector<double> costs;
};

}  // namespace

YinAnalysis Yin(dsp::IFft& fft, std::span<const double> x) {
  YinAnalysis out;
  // Sample i of x is at index i + kYinLead + 1 of the padded signal; the
  // padding after it covers the longest lag.
  std::vector<double> pad(kYinLead + 1 + x.size() + kYinLead + kYinSegment + 1,
                          0.0);
  std::ranges::copy(x, pad.begin() + kYinLead + 1);
  std::vector<double> seg(kYinFft, 0.0);
  std::vector<double> win(kYinFft, 0.0);
  std::vector<std::complex<double>> product(kYinFft / 2 + 1);
  std::vector<double> cs(kYinSegment + 1, 0.0);
  std::vector<double> d(kYinLags, 0.0);
  for (std::size_t c = 0; c < x.size(); c += kPitchHop) {
    out.centres.push_back(static_cast<int>(c));
    const std::size_t a = c + 1;
    std::copy_n(pad.begin() + static_cast<std::ptrdiff_t>(a), kYinSegment,
                seg.begin());
    std::copy_n(seg.begin(), kYinWindow, win.begin());
    const std::vector<std::complex<double>> spectrum = fft.Forward(seg);
    const std::vector<std::complex<double>> window = fft.Forward(win);
    for (std::size_t i = 0; i < product.size(); ++i) {
      // conj(W) X, in numpy's order of operations for a complex product.
      const double ar = window[i].real();
      const double ai = -window[i].imag();
      const double br = spectrum[i].real();
      const double bi = spectrum[i].imag();
      product[i] = {(ar * br) - (ai * bi), (ar * bi) + (ai * br)};
    }
    const std::vector<double> r = fft.Inverse(product, kYinFft);

    double sum = 0.0;
    for (std::size_t i = 0; i < kYinSegment; ++i) {
      sum += seg[i] * seg[i];
      cs[i + 1] = sum;
    }
    const double e0 = cs[kYinWindow];
    for (std::size_t t = 0; t < kYinLags; ++t) {
      const double et = cs[t + kYinWindow] - cs[t];
      d[t] = e0 + et - (2.0 * r[t]);
    }
    d[0] = 0.0;
    std::vector<double> row(kYinLags, 1.0);
    double cm = 0.0;
    for (std::size_t t = 1; t < kYinLags; ++t) {
      cm += d[t];
      row[t] = cm > 0.0 ? d[t] * static_cast<double>(t) / cm : 1.0;
    }
    out.cmndf.push_back(std::move(row));
  }
  return out;
}

std::vector<PitchCandidate> PitchCandidates(std::span<const double> cmndf) {
  std::vector<PitchCandidate> c;
  const int top =
      std::min(kCandidateMaxLag, static_cast<int>(cmndf.size()) - 2);
  for (int t = std::max(kCandidateMinLag, 2); t <= top; ++t) {
    const auto at = static_cast<std::size_t>(t);
    const double a = cmndf[at - 1];
    const double b = cmndf[at];
    const double cc = cmndf[at + 1];
    if (b <= a && b < cc) {
      const double den = a - (2.0 * b) + cc;
      double off = den > 0.0 ? 0.5 * (a - cc) / den : 0.0;
      off = std::max(-kParabolaLimit, std::min(kParabolaLimit, off));
      c.push_back({.lag = t + off, .value = b - (0.25 * (a - cc) * off)});
    }
  }
  std::ranges::stable_sort(c, {}, &PitchCandidate::value);
  if (c.size() > kMaxCandidates) {
    c.resize(kMaxCandidates);
  }
  return c;
}

VoicingEvidence MeasureVoicingEvidence(std::span<const double> x,
                                       std::span<const int> centres) {
  constexpr auto kWin = static_cast<std::size_t>(kEvidenceWindow);
  std::vector<double> pad(x.size() + (2 * kWin), 0.0);
  std::vector<double> padp(pad.size(), 0.0);
  for (std::size_t i = 0; i < x.size(); ++i) {
    pad[kWin + i] = x[i];
    padp[kWin + i] = i == 0 ? x[0] : x[i] - (kEvidencePreEmphasis * x[i - 1]);
  }
  const std::vector<double> ham = dsp::HammingWindow(kEvidenceWindow);
  VoicingEvidence out;
  std::vector<double> s(kWin);
  for (const int c : centres) {
    const std::size_t a = static_cast<std::size_t>(c) + kWin - (kWin / 2);
    for (std::size_t i = 0; i < kWin; ++i) {
      s[i] = padp[a + i] * ham[i];
    }
    const double r0 = dsp::Dot(s, s);
    const std::span<const double> sv(s);
    const double q =
        r0 > 0.0 ? dsp::Dot(sv.first(kWin - 1), sv.subspan(1)) / r0 : 0.0;
    double tilt = 4.0;
    if (q <= -0.5) {
      tilt = -2.0;
    } else if (q < 0.25) {
      tilt = -1.0;
    } else if (q < 0.5) {
      tilt = 1.0;
    } else if (q < 0.75) {
      tilt = 2.0;
    }
    out.tilt.push_back(tilt);
    int crossings = 0;
    for (std::size_t i = 0; i + 1 < kWin; ++i) {
      const double p = pad[a + i];
      const double n = pad[a + i + 1];
      const double sp = p > 0.0 ? 1.0 : (p < 0.0 ? -1.0 : 0.0);
      const double sn = n > 0.0 ? 1.0 : (n < 0.0 ? -1.0 : 0.0);
      if (sp * sn < 0.0) {
        ++crossings;
      }
    }
    out.crossings.push_back(static_cast<double>(crossings) * kSamplesPer10Ms /
                            static_cast<double>(kWin));
  }
  return out;
}

YinPitchTracker::YinPitchTracker(dsp::IFft& fft) : fft_(&fft) {}

PitchTrack YinPitchTracker::Track(std::span<const double> x,
                                  const EncoderOptions& options) {
  const auto log = Logger(kLogModuleEncoder);
  const std::vector<double> xl = LowPass1kHz(x);
  YinAnalysis yin = Yin(*fft_, xl);
  const std::vector<int>& centres = yin.centres;
  const std::size_t nh = centres.size();
  PitchTrack track{.hops = centres, .periods = {}};
  if (nh == 0) {
    return track;
  }
  const std::vector<double> rms = WindowRms(x, centres);
  const std::vector<double> rms_low = WindowRms(xl, centres);
  std::vector<double> positive;
  for (const double v : rms) {
    if (v > 0.0) {
      positive.push_back(v);
    }
  }
  const double loud =
      positive.empty() ? 1.0 : dsp::Percentile(positive, kLoudPercentile);

  std::vector<std::vector<PitchCandidate>> cands;
  cands.reserve(nh);
  std::vector<double> firsts;
  for (std::size_t h = 0; h < nh; ++h) {
    const std::vector<double>& row = yin.cmndf[h];
    cands.push_back(PitchCandidates(row));
    if (rms[h] < loud * kMedianQuiet) {
      continue;
    }
    for (int t = kCandidateMinLag; t <= kCandidateMaxLag; ++t) {
      const auto at = static_cast<std::size_t>(t);
      if (row[at] < kConfidentDip && row[at] <= row[at + 1]) {
        firsts.push_back(static_cast<double>(t));
        break;
      }
    }
  }
  const double med = firsts.empty() ? kDefaultMedian : dsp::Median(firsts);
  log->debug("pitch: {} hops, loud {:.6g}, median period {:.4g}", nh, loud,
             med);

  const VoicingEvidence evidence = MeasureVoicingEvidence(x, centres);
  std::vector<HopStates> states(nh);
  for (std::size_t h = 0; h < nh; ++h) {
    const double lev = loud > 0.0 ? rms[h] / loud : 0.0;
    const double lfr = rms[h] > 0.0 ? rms_low[h] / rms[h] : 0.0;
    const double lev_db = 20.0 * std::log10(std::max(lev, kMinimumLevel));
    double th = options.voicing_threshold +
                (kLowPassRise * Clamp01((lfr - kLowPassFrom) / kRatioSpan) *
                 Clamp01((lev_db + kLevelFloorDb) / kLevelSpanDb)) -
                (kHighPassFall * Clamp01((kHighPassFrom - lfr) / kRatioSpan));
    th += options.voicing_evidence *
          (evidence.tilt[h] -
           ((evidence.crossings[h] - kCrossingsCentre) / kCrossingsScale)) /
          kEvidenceScale;
    HopStates& st = states[h];
    for (const PitchCandidate& cand : cands[h]) {
      st.lags.push_back(cand.lag);
      double c = kAlpha * cand.value;
      const double dev = std::abs(std::log2(cand.lag / med));
      c += kPrior * std::max(0.0, dev - kPriorFree);
      for (const PitchCandidate& other : cands[h]) {
        if (other.lag >= kCandidateMinLag && other.value < kConfidentDip &&
            other.value <= cand.value + kSubharmonicSlack) {
          const double ratio = cand.lag / other.lag;
          if (std::abs(ratio - 2.0) < kSubharmonicTolerance * 2.0 ||
              std::abs(ratio - 3.0) < kSubharmonicTolerance * 3.0) {
            c += kSubharmonic;
            break;
          }
        }
      }
      if (lev < kQuietLevel) {
        c += kQuietCost;
      }
      st.costs.push_back(c);
    }
    st.lags.push_back(0.0);
    st.costs.push_back(kAlpha * th);
    if (log->should_log(spdlog::level::trace)) {
      log->trace("pitch hop {}: {} candidates, unvoiced cost {:.6g}", h,
                 cands[h].size(), kAlpha * th);
    }
  }

  // Viterbi. Each state also carries the last voiced lag on its best path
  // and the hops since, so that a short unvoiced gap cannot be used to
  // change octave for less than a jump costs (an approximation to the full
  // search).
  struct LastVoiced {
    double lag = 0.0;
    int gap = 0;
  };
  std::vector<double> acc = states[0].costs;
  std::vector<LastVoiced> last;
  for (const double lag : states[0].lags) {
    last.push_back({.lag = lag, .gap = 0});
  }
  std::vector<std::vector<std::size_t>> back(nh);
  std::vector<double> next_acc;
  std::vector<LastVoiced> next_last;
  for (std::size_t h = 1; h < nh; ++h) {
    const std::vector<double>& prev = states[h - 1].lags;
    const HopStates& cur = states[h];
    next_acc.assign(cur.lags.size(), 0.0);
    next_last.assign(cur.lags.size(), {});
    back[h].assign(cur.lags.size(), 0);
    for (std::size_t j = 0; j < cur.lags.size(); ++j) {
      const double lj = cur.lags[j];
      double best = std::numeric_limits<double>::infinity();
      std::size_t bi = 0;
      for (std::size_t i = 0; i < prev.size(); ++i) {
        const double li = prev[i];
        double t = 0.0;
        if (li > 0.0 && lj > 0.0) {
          t = kJump * std::abs(std::log2(lj / li));
        } else if (li > 0.0 || lj > 0.0) {
          t = kSwitch;
          const LastVoiced& lv = last[i];
          if (lj > 0.0 && lv.lag > 0.0 && lv.gap < kGapHops) {
            t += kJump * std::abs(std::log2(lj / lv.lag)) *
                 (1.0 - (static_cast<double>(lv.gap) / kGapHops));
          }
        }
        const double v = acc[i] + t;
        if (i == 0 || v < best) {
          best = v;
          bi = i;
        }
      }
      next_acc[j] = best + cur.costs[j];
      back[h][j] = bi;
      next_last[j] =
          lj > 0.0 ? LastVoiced{.lag = lj, .gap = 0}
                   : LastVoiced{.lag = last[bi].lag, .gap = last[bi].gap + 1};
    }
    acc.swap(next_acc);
    last.swap(next_last);
  }
  std::size_t j =
      static_cast<std::size_t>(std::ranges::min_element(acc) - acc.begin());
  track.periods.assign(nh, 0.0);
  for (std::size_t h = nh; h-- > 0;) {
    track.periods[h] = states[h].lags[j];
    if (h > 0) {
      j = back[h][j];
    }
  }
  return track;
}

}  // namespace speak5220::core::encoder
