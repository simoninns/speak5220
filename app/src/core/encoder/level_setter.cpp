/*
 * File:        level_setter.cpp
 * Module:      encoder
 * Purpose:     Sets each frame's energy: level shaping, an open-loop
 *              estimate, closed-loop passes and the peak limiter
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/level_setter.h"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/fft.h"
#include "core/dsp/lpc_analysis.h"
#include "core/encoder/encoder_filters.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_analyser.h"
#include "core/encoder/frame_endings.h"
#include "core/encoder/quantiser.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"
#include "core/synth/reference_synthesizer.h"
#include "core/util/logging.h"

namespace speak5220::core::encoder {
namespace {

// _gainOf(): the FFT size, the noise excitation's RMS and the output scale
// (§12.3: code = E x h / 128).
constexpr std::size_t kGainFft = 4096;
constexpr double kNoiseRms = 64.0;
constexpr double kCodeScale = 128.0;
constexpr std::size_t kChirpLength = 21;
// The K tables hold values scaled by 512 (§8.1, Table 8-6).
constexpr double kKScale = 512.0;

// _shapeLevels(): the slow gain control window, in frames (0.75 s).
constexpr int kAgcFrames = 30;
constexpr double kLevelFloor = 1e-9;
constexpr double kEnvelopeFloor = 1e-18;

// _setLevels(): the closed loop leaves frames within kSettledDb alone and
// moves the rest kStepFraction of the way (in dB) to their target.
constexpr double kMinimumLevel = 1e-6;
constexpr double kSettledDb = 1.5;
constexpr double kStepFraction = 0.7;
// The closed loop's passes (tms5220enc.py DEFAULTS "iterations"): more
// change no corpus file's score by more than 0.02 (HLD §8, "Options").
constexpr int kLevelPasses = 5;
// A frame this many times too loud at energy 1 becomes silence.
constexpr double kTooLoud = 4.0;
// The limiter. The reference holds every output code within 120
// (tms5220enc.py DEFAULTS "peakLimit"); Acorn's PHROM A clips the DAC on
// the peaks of half its words, 0.21 % of its samples on average
// (docs-tech/encoder-quality-investigation.md §4.1). So no frame's peak
// may pass kPeakCeiling, and at most kClipBudget of the phrase's samples
// may pass the DAC's +-127 (speak5220 datasheet §8.9), which clips them.
constexpr double kPeakCeiling = 160.0;
constexpr int kDacLimit = 127;
constexpr double kClipBudget = 0.001;
// The limiter's passes, its peak window, and dB per energy step.
constexpr int kLimiterPasses = 30;
constexpr int kLimiterPeakHalf = 110;
constexpr double kDbPerEnergyStep = 3.0;
constexpr int kSafetyPasses = 14;

std::vector<double> PowerSpectrum(dsp::IFft& fft, std::span<const double> x) {
  std::vector<double> padded(kGainFft, 0.0);
  std::copy_n(x.begin(), std::min(x.size(), kGainFft), padded.begin());
  const std::vector<std::complex<double>> bins = fft.Forward(padded);
  std::vector<double> power(bins.size());
  for (std::size_t i = 0; i < bins.size(); ++i) {
    const double magnitude = std::hypot(bins[i].real(), bins[i].imag());
    power[i] = magnitude * magnitude;
  }
  return power;
}

// numpy.mean(numpy.concatenate([v, v[1:-1]])): the mean over a full
// period of a real spectrum's power.
double FullSpectrumMean(std::span<const double> v) {
  std::vector<double> full(v.begin(), v.end());
  full.insert(full.end(), v.begin() + 1, v.end() - 1);
  return dsp::Mean(full);
}

// numpy.convolve(x, w, "full")[a:a + len(x)] with a = (len(w) - 1) // 2,
// each output summed in the order of the longer input, as numpy does.
// Re-implements tms5220enc.py _smoothed().
std::vector<double> Smoothed(std::span<const double> x,
                             std::span<const double> w) {
  const auto nx = static_cast<std::ptrdiff_t>(x.size());
  const auto nw = static_cast<std::ptrdiff_t>(w.size());
  const std::ptrdiff_t a = (nw - 1) / 2;
  std::vector<double> out(x.size(), 0.0);
  for (std::ptrdiff_t i = 0; i < nx; ++i) {
    const std::ptrdiff_t n = a + i;
    // out = sum over k of x[k] w[n - k], for 0 <= k < nx, 0 <= n - k < nw.
    const std::ptrdiff_t k_lo = std::max<std::ptrdiff_t>(0, n - nw + 1);
    const std::ptrdiff_t k_hi = std::min(nx - 1, n);
    double sum = 0.0;
    if (nx >= nw) {
      for (std::ptrdiff_t k = k_lo; k <= k_hi; ++k) {
        sum +=
            x[static_cast<std::size_t>(k)] * w[static_cast<std::size_t>(n - k)];
      }
    } else {
      for (std::ptrdiff_t k = k_hi; k >= k_lo; --k) {
        sum +=
            w[static_cast<std::size_t>(n - k)] * x[static_cast<std::size_t>(k)];
      }
    }
    out[static_cast<std::size_t>(i)] = sum;
  }
  return out;
}

int EnergyValue(int index) {
  return lpc::kEnergyTable.at(static_cast<std::size_t>(index));
}

int MaxAbs(std::span<const int> codes) {
  int peak = 0;
  for (const int c : codes) {
    peak = std::max(peak, std::abs(c));
  }
  return peak;
}

// The samples of `codes` beyond the DAC's range.
int ClippedSamples(std::span<const int> codes) {
  return static_cast<int>(std::ranges::count_if(
      codes, [](int c) { return std::abs(c) > kDacLimit; }));
}

// The samples the clip budget allows in `codes`.
int AllowedClips(std::span<const int> codes) {
  return static_cast<int>(
      std::floor(kClipBudget * static_cast<double>(codes.size())));
}

// Whether `out` is within the limits: no lattice wrap (speak5220
// datasheet §12.3), no code beyond kPeakCeiling, and the clip budget.
bool WithinLimits(const synth::ReferenceSynthesis& out) {
  return out.wrapped_samples == 0 && MaxAbs(out.codes) <= kPeakCeiling &&
         ClippedSamples(out.codes) <= AllowedClips(out.codes);
}

synth::ReferenceSynthesis Render(std::span<const PlanEntry> plan,
                                 std::span<const int> energies) {
  return synth::ReferenceSynthesizer::Synthesize(
      AddFades(ToFrames(plan, energies)));
}

}  // namespace

double GainOf(dsp::IFft& fft, std::span<const double> k_values, int period,
              bool loudness) {
  const std::vector<double> a = dsp::StepUp(k_values);
  std::vector<double> h = PowerSpectrum(fft, a);
  for (double& v : h) {
    v = 1.0 / v;
  }
  if (loudness) {
    static const std::vector<double> kWeight =
        LoudnessHighPassPower((kGainFft / 2) + 1);
    assert(h.size() == kWeight.size());
    for (std::size_t i = 0; i < h.size(); ++i) {
      h[i] *= kWeight[i];
    }
  }
  if (period == 0) {
    return kNoiseRms * std::sqrt(FullSpectrumMean(h)) / kCodeScale;
  }
  std::vector<double> chirp;
  chirp.reserve(kChirpLength);
  for (std::size_t i = 0; i < kChirpLength; ++i) {
    chirp.push_back(static_cast<double>(lpc::kChirpTable[i]));
  }
  std::vector<double> ch = PowerSpectrum(fft, chirp);
  for (std::size_t i = 0; i < ch.size(); ++i) {
    ch[i] *= h[i];
  }
  // The mean over the spectrum is the sum over one chirp of (c * h)^2.
  const double energy = FullSpectrumMean(ch);
  return std::sqrt(energy / static_cast<double>(period)) / kCodeScale;
}

std::vector<std::vector<double>> EffectiveK(std::span<const PlanEntry> plan) {
  std::vector<std::vector<double>> out;
  out.reserve(plan.size());
  std::vector<int> last(lpc::kKCount, 0);
  bool last_voiced = true;
  for (const PlanEntry& p : plan) {
    if (p.kind == PlanKind::kVoiced || p.kind == PlanKind::kUnvoiced) {
      last.assign(lpc::kKCount, 0);
      std::ranges::copy(p.k, last.begin());
      last_voiced = p.kind == PlanKind::kVoiced;
    }
    if (p.kind == PlanKind::kSilence) {
      out.emplace_back();
      continue;
    }
    const int n =
        (last_voiced && p.pitch != 0) ? lpc::kKCount : lpc::kUnvoicedKCount;
    std::vector<double> values;
    for (int i = 0; i < n; ++i) {
      const auto at = static_cast<std::size_t>(i);
      values.push_back(static_cast<double>(
                           lpc::KTable(i)[static_cast<std::size_t>(last[at])]) /
                       kKScale);
    }
    out.push_back(std::move(values));
  }
  return out;
}

OutputLevels MeasureOutputLevels(std::span<const int> codes, int frames,
                                 int peak_half) {
  const std::vector<double> c(codes.begin(), codes.end());
  OutputLevels out;
  out.level.assign(static_cast<std::size_t>(frames), 0.0);
  out.peak.assign(static_cast<std::size_t>(frames), 0);
  const auto length = static_cast<int>(codes.size());
  for (int j = 0; j < frames; ++j) {
    const int m = kReachedSample + (kFrameSamples * j);
    const auto at = static_cast<std::size_t>(j);
    out.level[at] = FrameRms(c, m);
    const int a = std::max(0, m - peak_half);
    const int b = std::min(length, m + peak_half);
    if (b > a) {
      out.peak[at] = MaxAbs(codes.subspan(static_cast<std::size_t>(a),
                                          static_cast<std::size_t>(b - a)));
    }
  }
  return out;
}

OutputLevels MeasureOutputLoudness(std::span<const int> codes, int frames,
                                   int peak_half) {
  OutputLevels out = MeasureOutputLevels(codes, frames, peak_half);
  const std::vector<double> c(codes.begin(), codes.end());
  const std::vector<double> weighted = LoudnessHighPass(c);
  for (int j = 0; j < frames; ++j) {
    out.level[static_cast<std::size_t>(j)] =
        FrameRms(weighted, kReachedSample + (kFrameSamples * j));
  }
  return out;
}

LevelSetter::LevelSetter(dsp::IFft& fft) : fft_(&fft) {}

std::vector<double> LevelSetter::ShapeLevels(std::span<const double> levels,
                                             std::span<const PlanEntry> plan,
                                             const EncoderOptions& options) {
  assert(levels.size() == plan.size());
  const double agc = options.level_agc;
  const std::size_t n = plan.size();
  std::vector<bool> s(n);
  bool any = false;
  for (std::size_t j = 0; j < n; ++j) {
    s[j] = plan[j].kind != PlanKind::kSilence;
    any = any || s[j];
  }
  std::vector<double> out(levels.begin(), levels.end());
  if (agc == 1.0 || !any) {
    return out;
  }
  std::vector<double> db(n);
  for (std::size_t j = 0; j < n; ++j) {
    db[j] = 20.0 * std::log10(std::max(levels[j], kLevelFloor));
  }
  const std::vector<double> w = dsp::InnerHannWindow(kAgcFrames);
  std::vector<double> power(n);
  std::vector<double> mask(n);
  for (std::size_t j = 0; j < n; ++j) {
    power[j] = s[j] ? std::pow(10.0, db[j] / 10.0) : 0.0;
    mask[j] = s[j] ? 1.0 : 0.0;
  }
  const std::vector<double> num = Smoothed(power, w);
  const std::vector<double> den = Smoothed(mask, w);
  std::vector<double> env(n);
  std::vector<double> sounding_env;
  for (std::size_t j = 0; j < n; ++j) {
    env[j] = 10.0 * std::log10(std::max(num[j] / std::max(den[j], kLevelFloor),
                                        kEnvelopeFloor));
    if (s[j]) {
      sounding_env.push_back(env[j]);
    }
  }
  const double ref = dsp::Median(sounding_env);
  for (std::size_t j = 0; j < n; ++j) {
    if (s[j]) {
      db[j] = db[j] - ((env[j] - ref) * (1.0 - (1.0 / agc)));
    }
  }
  for (std::size_t j = 0; j < n; ++j) {
    out[j] = std::pow(10.0, db[j] / 20.0);
  }
  return out;
}

std::vector<int> LevelSetter::SetLevels(std::span<const PlanEntry> plan,
                                        std::span<const double> levels,
                                        double gain, LevelStats* stats) {
  assert(levels.size() == plan.size());
  const auto log = Logger(kLogModuleEncoder);
  const auto nfr = static_cast<int>(plan.size());
  const std::size_t n = plan.size();
  const std::vector<std::vector<double>> eff = EffectiveK(plan);
  std::vector<double> target(n);
  std::vector<int> energies(n, 0);
  std::vector<bool> sounding(n);
  for (std::size_t j = 0; j < n; ++j) {
    target[j] = levels[j] * gain;
    sounding[j] = plan[j].kind != PlanKind::kSilence;
    if (!sounding[j]) {
      continue;
    }
    const int pitch = plan[j].pitch;
    const int period =
        pitch != 0 ? lpc::kPitchTable.at(static_cast<std::size_t>(pitch)) + 1
                   : 0;
    const double g = GainOf(*fft_, eff[j], period, /*loudness=*/true);
    energies[j] = EnergyIndex(g > 0.0 ? target[j] / g : 1.0);
  }
  const bool any_sounding =
      std::ranges::any_of(sounding, [](bool v) { return v; });

  // Closed loop: keep the pass with the lowest mean level error.
  std::optional<double> best_score;
  std::vector<int> best;
  std::vector<double> err(n);
  std::vector<double> abs_err;
  for (int it = 0; it < kLevelPasses; ++it) {
    const synth::ReferenceSynthesis out = Render(plan, energies);
    const OutputLevels lev = MeasureOutputLoudness(out.codes, nfr);
    abs_err.clear();
    for (std::size_t j = 0; j < n; ++j) {
      err[j] = 20.0 * std::log10(std::max(target[j], kMinimumLevel) /
                                 std::max(lev.level[j], kMinimumLevel));
      if (sounding[j]) {
        abs_err.push_back(std::abs(err[j]));
      }
    }
    const double score = any_sounding ? dsp::Mean(abs_err) : 0.0;
    log->debug("levels: pass {}, mean error {:.4f} dB", it, score);
    if (!best_score.has_value() || score < *best_score) {
      best_score = score;
      best = energies;
    }
    if (it == kLevelPasses - 1) {
      break;
    }
    std::vector<int> next = energies;
    for (std::size_t j = 0; j < n; ++j) {
      if (!sounding[j] || energies[j] == 0 || std::abs(err[j]) < kSettledDb) {
        continue;
      }
      const double want = static_cast<double>(EnergyValue(energies[j])) *
                          std::pow(10.0, kStepFraction * err[j] / 20.0);
      next[j] = EnergyIndex(want);
    }
    if (next == energies) {
      break;
    }
    energies = std::move(next);
  }
  energies = best;

  // Frames that are far too loud even at index 1 become silence.
  int silenced = 0;
  {
    const synth::ReferenceSynthesis out = Render(plan, energies);
    const OutputLevels lev = MeasureOutputLoudness(out.codes, nfr);
    for (std::size_t j = 1; j < n; ++j) {
      if (sounding[j] && energies[j] == 1 &&
          lev.level[j] > target[j] * kTooLoud) {
        energies[j] = 0;
        ++silenced;
      }
    }
  }
  const std::vector<int> before_limit = energies;

  // Peak, clip and wrap limiting: lower each frame whose peak passes the
  // ceiling to it; while the phrase clips more samples than the budget,
  // lower by a step the frames whose windows clip the most, enough of them
  // to cover the excess; a peak or wrap in no frame's window lowers the
  // loudest frame.
  for (int pass = 0; pass < kLimiterPasses; ++pass) {
    const synth::ReferenceSynthesis out = Render(plan, energies);
    if (WithinLimits(out)) {
      break;
    }
    const OutputLevels lev =
        MeasureOutputLevels(out.codes, nfr, kLimiterPeakHalf);
    std::vector<int> drop(n, 0);
    bool any = false;
    for (std::size_t j = 0; j < n; ++j) {
      if (sounding[j] && lev.peak[j] > kPeakCeiling) {
        const double ratio = static_cast<double>(lev.peak[j]) / kPeakCeiling;
        drop[j] =
            std::max(1, static_cast<int>(std::ceil(20.0 * std::log10(ratio) /
                                                   kDbPerEnergyStep)));
        any = true;
      }
    }
    const int excess = ClippedSamples(out.codes) - AllowedClips(out.codes);
    if (excess > 0) {
      // (clipped samples in the frame's window, frame), most first; ties
      // to the earlier frame.
      std::vector<std::pair<int, std::size_t>> clipping;
      for (std::size_t j = 0; j < n; ++j) {
        if (!sounding[j] || lev.peak[j] <= kDacLimit) {
          continue;
        }
        const int m = kReachedSample + (kFrameSamples * static_cast<int>(j));
        const int a = std::max(0, m - kLimiterPeakHalf);
        const int b =
            std::min(static_cast<int>(out.codes.size()), m + kLimiterPeakHalf);
        const int count =
            ClippedSamples(std::span<const int>(out.codes).subspan(
                static_cast<std::size_t>(a), static_cast<std::size_t>(b - a)));
        clipping.emplace_back(count, j);
      }
      std::ranges::stable_sort(clipping, [](const auto& x, const auto& y) {
        return x.first > y.first;
      });
      int covered = 0;
      for (const auto& [count, j] : clipping) {
        if (covered >= excess) {
          break;
        }
        drop[j] = std::max(drop[j], 1);
        covered += count;
        any = true;
      }
    }
    if (!any) {
      // A peak outside every frame's window: lower the loudest frame.
      std::size_t loudest = 0;
      double loudest_level = 0.0;
      for (std::size_t j = 0; j < n; ++j) {
        const double v = sounding[j] ? lev.level[j] : -1.0;
        if (j == 0 || v > loudest_level) {
          loudest_level = v;
          loudest = j;
        }
      }
      drop[loudest] = 1;
    }
    for (std::size_t j = 0; j < n; ++j) {
      if (drop[j] == 0) {
        continue;
      }
      if (energies[j] > 1) {
        energies[j] = std::max(1, energies[j] - drop[j]);
      } else if (j > 0) {
        energies[j] = 0;
      }
    }
  }
  // Safety net: lower everything until the output is within the limits.
  for (int pass = 0; pass < kSafetyPasses; ++pass) {
    const synth::ReferenceSynthesis out = Render(plan, energies);
    if (WithinLimits(out)) {
      break;
    }
    for (int& e : energies) {
      if (e > 1) {
        --e;
      }
    }
  }

  if (stats != nullptr) {
    int limited = 0;
    int dropped = 0;
    int count = 0;
    for (std::size_t j = 0; j < n; ++j) {
      dropped += (energies[j] == 0 && before_limit[j] > 0) ? 1 : 0;
      limited += (energies[j] < before_limit[j] && energies[j] > 0) ? 1 : 0;
      count += sounding[j] ? 1 : 0;
    }
    stats->silenced = silenced + dropped;
    stats->limited = limited;
    stats->sounding = count;
  }
  return energies;
}

}  // namespace speak5220::core::encoder
