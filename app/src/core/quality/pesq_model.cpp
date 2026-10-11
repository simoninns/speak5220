/*
 * File:        pesq_model.cpp
 * Module:      quality
 * Purpose:     PESQ's perceptual model: the auditory transform, the
 *              disturbance densities, bad-interval realignment and the
 *              aggregation to a score (ITU-T P.862 §10.2)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "core/quality/pesq_fft.h"
#include "core/quality/pesq_stages.h"
#include "core/quality/pesq_tables.h"
#include "core/util/status.h"

namespace speak5220::core::quality::pesq {

namespace t = pesq_tables;

namespace {

constexpr double kTwoPi = 6.28318530717959;
constexpr int kNf = t::kFrameSize;
constexpr int kHop = kNf / 2;
constexpr std::size_t kBands = t::kBands;

// ITU-T P.862 §10.2.3: the active interval starts and ends where five
// successive samples sum (in magnitude) to this.
constexpr float kSilenceOf5Samples = 500.0F;

// ITU-T P.862 §10.2.13: frames disturbed by more than this are bad.
constexpr float kBadFrameThreshold = 30.0F;
// Bad frames are smeared over this many frames either side, and a bad
// interval needs this many frames.
constexpr int kSmearRange = 2;
constexpr int kMinBadIntervalFrames = 5;
// A bad interval is realigned within this many frame lengths.
constexpr int kBadSearchRangeFrames = 4;
// Below this correlation a bad interval is noise against noise.
constexpr float kMinBadCorrelation = 0.5F;

// ITU-T P.862 §10.2.7: the bounds of the short-term gain compensation.
constexpr float kMaxScale = 5.0F;
constexpr float kMinScale = 3E-4F;

// ITU-T P.862 §10.2.11: frame disturbances are limited to this.
constexpr float kMaxFrameDisturbance = 45.0F;

// ITU-T P.862 §10.2.14: frames per split-second interval.
constexpr int kFramesPerSyllable = 20;

std::size_t Index(int i) { return static_cast<std::size_t>(i); }

// Frame `frame`'s band densities in a frames x bands array.
std::span<float> FrameOf(std::vector<float>& densities, int frame) {
  return std::span(densities).subspan(Index(frame) * kBands, kBands);
}
std::span<const float> FrameOf(const std::vector<float>& densities, int frame) {
  return std::span(densities).subspan(Index(frame) * kBands, kBands);
}

// ITU-T P.862 §10.2.4: the power spectrum of one Hann-windowed frame,
// bins 0..kNf/2 - 1, with the DC bin cleared.
void ShortTermSpectrum(std::span<const float> data, int start,
                       std::span<const float> window, std::span<float> spectrum,
                       std::span<float> fft) {
  for (std::size_t n = 0; n < kNf; ++n) {
    fft[n] = data[Index(start) + n] * window[n];
  }
  RealFft(fft, kNf);
  for (std::size_t k = 0; k < kNf / 2; ++k) {
    spectrum[k] =
        fft[k << 1U] * fft[k << 1U] + fft[1 + (k << 1U)] * fft[1 + (k << 1U)];
  }
  spectrum[0] = 0;
}

// The disturbance densities of one frame, from the two pitch power
// densities, once the degraded one has been gain-compensated: the
// symmetric and asymmetric frame disturbances (ITU-T P.862 §10.2.8 to
// §10.2.11, before the weighting of soft frames).
struct FrameDisturbance {
  float symmetric = 0.0F;
  float asymmetric = 0.0F;
};

FrameDisturbance DisturbanceOf(std::span<const float> ref_density,
                               std::span<const float> deg_density) {
  std::array<float, kBands> loudness_ref{};
  std::array<float, kBands> loudness_deg{};
  std::array<float, kBands> disturbance{};
  IntensityWarp(ref_density, loudness_ref);
  IntensityWarp(deg_density, loudness_deg);
  // ITU-T P.862 §10.2.9: the difference, pulled towards zero by a quarter
  // of the smaller loudness (masking).
  for (std::size_t band = 0; band < kBands; ++band) {
    const float d = loudness_deg[band] - loudness_ref[band];
    float m = std::min(loudness_deg[band], loudness_ref[band]);
    m *= 0.25F;
    if (d > m) {
      disturbance[band] = d - m;
    } else if (d < -m) {
      disturbance[band] = d + m;
    } else {
      disturbance[band] = 0;
    }
  }
  FrameDisturbance f;
  f.symmetric = PseudoLp(disturbance, t::kDisturbancePowerFrequency);
  MultiplyByAsymmetry(disturbance, ref_density, deg_density);
  f.asymmetric = PseudoLp(disturbance, t::kAsymmetryPowerFrequency);
  return f;
}

// ITU-T P.862 §10.2.7: scales the degraded densities of frames [first,
// last) towards the reference's audible power, the ratio low-pass filtered
// along time, then calls on_frame(frame, audible reference power).
template <typename OnFrame>
void CompensateGain(int first, int last, const std::vector<float>& ref,
                    std::vector<float>& deg, OnFrame on_frame) {
  float old_scale = 1;
  for (int frame = first; frame < last; ++frame) {
    const float audible_ref = TotalAudible(FrameOf(ref, frame), 1);
    const float audible_deg = TotalAudible(FrameOf(deg, frame), 1);
    float scale = (audible_ref + 5E3F) / (audible_deg + 5E3F);
    if (frame > 0) {
      scale = 0.2F * old_scale + 0.8F * scale;
    }
    old_scale = scale;
    scale = std::clamp(scale, kMinScale, kMaxScale);
    for (float& v : FrameOf(deg, frame)) {
      v *= scale;
    }
    on_frame(frame, audible_ref);
  }
}

}  // namespace

void FrequencyWarp(std::span<const float> hz_spectrum,
                   std::span<float> pitch_power_density) {
  std::size_t hz_band = 0;
  for (std::size_t band = 0; band < kBands; ++band) {
    double sum = 0;
    for (int i = 0; i < t::kBinsPerBand[band]; ++i) {
      sum += hz_spectrum[hz_band++];
    }
    sum *= t::kPowerDensityCorrection[band];
    sum *= static_cast<double>(static_cast<float>(t::kPowerScale));
    pitch_power_density[band] = static_cast<float>(sum);
  }
}

float TotalAudible(std::span<const float> pitch_power_density, float factor) {
  double result = 0.;
  for (std::size_t band = 1; band < kBands; ++band) {
    const float h = pitch_power_density[band];
    const auto threshold =
        static_cast<float>(factor * t::kAbsoluteThreshold[band]);
    if (h > threshold) {
      result += h;
    }
  }
  return static_cast<float>(result);
}

void IntensityWarp(std::span<const float> pitch_power_density,
                   std::span<float> loudness_density) {
  const auto loudness_scale = static_cast<float>(t::kLoudnessScale);
  for (std::size_t band = 0; band < kBands; ++band) {
    const auto threshold = static_cast<float>(t::kAbsoluteThreshold[band]);
    const float input = pitch_power_density[band];
    // ITU-T P.862 §10.2.8: below 4 Bark the power is raised slightly, for
    // recruitment.
    const auto centre = static_cast<float>(t::kBandCentreBark[band]);
    float h = centre < 4.0F ? 6.0F / (centre + 2.0F) : 1.0F;
    h = std::min(h, 2.0F);
    h = static_cast<float>(
        std::pow(static_cast<double>(h), static_cast<double>(0.15F)));
    const double power = t::kZwickerPower * h;
    if (input > threshold) {
      loudness_density[band] = static_cast<float>(
          std::pow(threshold / 0.5, power) *
          (std::pow(0.5 + 0.5 * input / threshold, power) - 1));
    } else {
      loudness_density[band] = 0;
    }
    loudness_density[band] *= loudness_scale;
  }
}

float PseudoLp(std::span<const float> x, float p) {
  double total_weight = 0;
  double result = 0;
  for (std::size_t band = 1; band < kBands; ++band) {
    const float h = std::fabs(x[band]);
    const auto w = static_cast<float>(t::kBandWidthBark[band]);
    const float prod = h * w;
    result += std::pow(static_cast<double>(prod), static_cast<double>(p));
    total_weight += w;
  }
  result /= total_weight;
  result = std::pow(result, 1 / static_cast<double>(p));
  result *= total_weight;
  return static_cast<float>(result);
}

void MultiplyByAsymmetry(std::span<float> disturbance,
                         std::span<const float> ref_density,
                         std::span<const float> deg_density) {
  for (std::size_t i = 0; i < kBands; ++i) {
    const float ratio = (deg_density[i] + 50.0F) / (ref_density[i] + 50.0F);
    auto h = static_cast<float>(
        std::pow(static_cast<double>(ratio), static_cast<double>(1.2F)));
    if (h > 12.0F) {
      h = 12.0F;
    }
    if (h < 3.0F) {
      h = 0.0F;
    }
    disturbance[i] *= h;
  }
}

float LpqWeight(int start_frame, int stop_frame, float power_syllable,
                float power_time, std::span<const float> frame_disturbance,
                std::span<const float> time_weight) {
  double result_time = 0;
  double total_time_weight = 0;
  for (int syllable_start = start_frame; syllable_start <= stop_frame;
       syllable_start += kFramesPerSyllable / 2) {
    double result_syllable = 0;
    int count_syllable = 0;
    for (int frame = syllable_start;
         frame < syllable_start + kFramesPerSyllable; ++frame) {
      if (frame <= stop_frame) {
        const float h = frame_disturbance[Index(frame)];
        result_syllable += std::pow(static_cast<double>(h),
                                    static_cast<double>(power_syllable));
      }
      ++count_syllable;
    }
    result_syllable /= count_syllable;
    result_syllable =
        std::pow(result_syllable, 1.0 / static_cast<double>(power_syllable));
    // As the reference: the weights are indexed from the first frame.
    const double weight = time_weight[Index(syllable_start - start_frame)];
    result_time +=
        std::pow(weight * result_syllable, static_cast<double>(power_time));
    total_time_weight += std::pow(weight, static_cast<double>(power_time));
  }
  result_time /= total_time_weight;
  result_time = std::pow(result_time, static_cast<double>(1.0F / power_time));
  return static_cast<float>(result_time);
}

DelayEstimate ComputeDelay(int start, int stop, int search_range,
                           std::span<const float> x1,
                           std::span<const float> x2) {
  const int n = stop - start;
  const int pow2 = NextPowerOfTwo(2 * n);
  // As the reference: multiplied by n, then divided by the transform size.
  const double power1 = PowerOf(x1, start, stop, n) * static_cast<double>(n) /
                        static_cast<double>(pow2);
  const double power2 = PowerOf(x2, start, stop, n) * static_cast<double>(n) /
                        static_cast<double>(pow2);
  const double normalization = std::sqrt(power1 * power2);
  DelayEstimate best;
  if ((power1 <= 1E-6) || (power2 <= 1E-6)) {
    return best;
  }
  std::vector<float> a(Index(pow2 + 2), 0.0F);
  std::vector<float> b(Index(pow2 + 2), 0.0F);
  std::vector<float> y(Index(pow2 + 2), 0.0F);
  for (int i = 0; i < n; ++i) {
    a[Index(i)] = std::fabs(x1[Index(i + start)]);
    b[Index(i)] = std::fabs(x2[Index(i + start)]);
  }
  RealFft(a, pow2);
  RealFft(b, pow2);
  const auto pow2_float = static_cast<float>(pow2);
  for (std::size_t i = 0; i <= Index(pow2 / 2); ++i) {
    a[2 * i] /= pow2_float;
    a[(2 * i) + 1] /= pow2_float;
  }
  for (std::size_t i = 0; i <= Index(pow2 / 2); ++i) {
    y[2 * i] = a[2 * i] * b[2 * i] + a[(2 * i) + 1] * b[(2 * i) + 1];
    y[(2 * i) + 1] = -a[(2 * i) + 1] * b[2 * i] + a[2 * i] * b[(2 * i) + 1];
  }
  RealIfft(y, pow2);
  auto consider = [&](int lag, float value) {
    const double h = static_cast<double>(std::fabs(value)) / normalization;
    if (std::fabs(h) > static_cast<double>(best.correlation)) {
      best.correlation = static_cast<float>(std::fabs(h));
      best.delay = lag;
    }
  };
  for (int i = -search_range; i <= -1; ++i) {
    consider(i, y[Index(i + pow2)]);
  }
  for (int i = 0; i < search_range; ++i) {
    consider(i, y[Index(i)]);
  }
  return best;
}

StatusOr<float> PerceptualModel(Signal& ref, Signal& deg,
                                const Alignment& alignment) {
  constexpr int kSb = t::kSearchBufferSamples;
  constexpr int kPad = t::kDataPaddingSamples;
  if (alignment.utterances < 1) {
    return FailedPreconditionError("PESQ found no utterances");
  }
  const int max_samples = std::max(ref.samples, deg.samples);
  std::array<float, kNf> window{};
  for (std::size_t n = 0; n < kNf; ++n) {
    window[n] = static_cast<float>(
        0.5 * (1.0 - std::cos((kTwoPi * static_cast<double>(n)) / kNf)));
  }

  // ITU-T P.862 §10.2.3: the active interval of the reference: skip
  // samples from each end while five successive ones sum to less than
  // kSilenceOf5Samples, up to half the signal.
  auto skip_silence = [&](auto sample_at) {
    int skip = 0;
    while (true) {
      float sum5 = 0.0F;
      for (int i = 0; i < 5; ++i) {
        sum5 += std::fabs(ref.data[Index(sample_at(skip, i))]);
      }
      if (sum5 >= kSilenceOf5Samples) {
        return skip;
      }
      ++skip;
      if (skip >= max_samples / 2) {
        return skip;
      }
    }
  };
  const int skip_start =
      skip_silence([](int skip, int i) { return kSb + skip + i; });
  const int skip_end = skip_silence(
      [&](int skip, int i) { return max_samples - kSb + kPad - 1 - skip - i; });
  const int start_frame = skip_start / kHop;
  const int stop_frame =
      ((max_samples - (2 * kSb) + kPad - skip_end) / kHop) - 1;
  if (stop_frame < start_frame || stop_frame < 0) {
    return FailedPreconditionError("PESQ found no active speech");
  }
  const int frames = stop_frame + 1;

  std::vector<float> fft(kNf + 2);
  std::vector<float> spectrum_ref(kNf / 2);
  std::vector<float> spectrum_deg(kNf / 2);
  std::vector<float> density_ref(Index(frames) * kBands);
  std::vector<float> density_deg(Index(frames) * kBands);
  std::vector<int> silent(Index(frames));
  std::vector<float> frame_disturbance(Index(frames));
  std::vector<float> frame_disturbance_asym(Index(frames));
  std::vector<float> total_power_ref(Index(frames));

  // The utterance a reference sample falls in, for its delay.
  auto delay_at = [&](int sample) {
    int utt = alignment.utterances - 1;
    while ((utt >= 0) &&
           (alignment.start[Index(utt)] * t::kDownsample > sample)) {
      --utt;
    }
    return utt >= 0 ? alignment.delay[Index(utt)] : alignment.delay[0];
  };

  // ITU-T P.862 §10.2.4-10.2.5: the pitch power densities, the degraded
  // frames shifted by their utterance's delay.
  for (int frame = 0; frame <= stop_frame; ++frame) {
    const int start_ref = kSb + (frame * kHop);
    ShortTermSpectrum(ref.data, start_ref, window, spectrum_ref, fft);
    const int start_deg = start_ref + delay_at(start_ref);
    if ((start_deg > 0) && (start_deg + kNf < max_samples + kPad)) {
      ShortTermSpectrum(deg.data, start_deg, window, spectrum_deg, fft);
    } else {
      std::ranges::fill(spectrum_deg, 0.0F);
    }
    FrequencyWarp(spectrum_ref, FrameOf(density_ref, frame));
    FrequencyWarp(spectrum_deg, FrameOf(density_deg, frame));
    silent[Index(frame)] = static_cast<int>(
        TotalAudible(FrameOf(density_ref, frame), 1E2F) < 1E7F);
  }

  // ITU-T P.862 §10.2.6: partial compensation of the reference for the
  // system's frequency response, from the spectra averaged over speech.
  const int average_frames = ((max_samples - (2 * kSb) + kPad) / kHop) - 1;
  auto average_audible = [&](const std::vector<float>& densities) {
    std::array<float, kBands> average{};
    for (std::size_t band = 0; band < kBands; ++band) {
      double result = 0;
      for (int frame = 0; frame < frames; ++frame) {
        if (silent[Index(frame)] == 0) {
          const float h = FrameOf(densities, frame)[band];
          if (h > 100 * t::kAbsoluteThreshold[band]) {
            result += h;
          }
        }
      }
      average[band] = static_cast<float>(result / average_frames);
    }
    return average;
  };
  const std::array<float, kBands> average_ref = average_audible(density_ref);
  const std::array<float, kBands> average_deg = average_audible(density_deg);
  for (std::size_t band = 0; band < kBands; ++band) {
    constexpr float kConstant = 1000;
    float x = (average_deg[band] + kConstant) / (average_ref[band] + kConstant);
    x = std::clamp(x, 0.01F, 100.0F);
    for (int frame = 0; frame < frames; ++frame) {
      FrameOf(density_ref, frame)[band] *= x;
    }
  }

  // ITU-T P.862 §10.2.7-10.2.11: gain compensation and the frame
  // disturbances.
  bool any_bad_frame = false;
  CompensateGain(
      0, frames, density_ref, density_deg, [&](int frame, float audible_ref) {
        total_power_ref[Index(frame)] = audible_ref;
        const FrameDisturbance f = DisturbanceOf(FrameOf(density_ref, frame),
                                                 FrameOf(density_deg, frame));
        frame_disturbance[Index(frame)] = f.symmetric;
        frame_disturbance_asym[Index(frame)] = f.asymmetric;
        if (f.symmetric > kBadFrameThreshold) {
          any_bad_frame = true;
        }
      });

  // ITU-T P.862 §10.2.12: frames where the delay fell by more than half
  // a window are ignored.
  for (int utt = 1; utt < alignment.utterances; ++utt) {
    const auto u = Index(utt);
    int frame1 = (((alignment.start[u] - t::kSearchBuffer) * t::kDownsample) +
                  alignment.delay[u]) /
                 kHop;
    const int j =
        (((alignment.end[u - 1] - t::kSearchBuffer) * t::kDownsample) +
         alignment.delay[u - 1]) /
        kHop;
    const int delay_jump = alignment.delay[u] - alignment.delay[u - 1];
    frame1 = std::max(std::min(frame1, j), 0);
    if (delay_jump < -kHop) {
      const int frame2 =
          ((((alignment.start[u] - t::kSearchBuffer) * t::kDownsample) +
            std::max(0, std::abs(delay_jump))) /
           kHop) +
          1;
      for (int frame = frame1; frame <= frame2; ++frame) {
        if (frame < stop_frame) {
          frame_disturbance[Index(frame)] = 0;
          frame_disturbance_asym[Index(frame)] = 0;
        }
      }
    }
  }

  // The degraded signal with each sample moved by its utterance's delay.
  const int nn = kPad + max_samples;
  std::vector<float> tweaked_deg(Index(nn), 0.0F);
  for (int i = kSb; i < nn - kSb; ++i) {
    const int j = std::clamp(i + delay_at(i), kSb, nn - kSb - 1);
    tweaked_deg[Index(i)] = deg.data[Index(j)];
  }

  // ITU-T P.862 §10.2.13: realign intervals of bad frames, and keep the
  // smaller disturbance.
  if (any_bad_frame) {
    std::vector<int> bad(Index(frames));
    std::vector<int> smeared(Index(frames), 0);
    for (int frame = 0; frame <= stop_frame; ++frame) {
      bad[Index(frame)] = static_cast<int>(frame_disturbance[Index(frame)] >
                                           kBadFrameThreshold);
    }
    bad[0] = 0;
    for (int frame = kSmearRange; frame < stop_frame - kSmearRange; ++frame) {
      int left = bad[Index(frame)];
      int right = bad[Index(frame)];
      for (int i = -kSmearRange; i <= 0; ++i) {
        left = std::max(left, bad[Index(frame + i)]);
      }
      for (int i = 0; i <= kSmearRange; ++i) {
        right = std::max(right, bad[Index(frame + i)]);
      }
      smeared[Index(frame)] = std::min(left, right);
    }
    struct BadInterval {
      int start_frame = 0;
      int stop_frame = 0;
      int start_sample = 0;
      int stop_sample = 0;
      int delay = 0;
    };
    std::vector<BadInterval> intervals;
    int frame = 0;
    while (frame <= stop_frame) {
      while ((frame <= stop_frame) && (smeared[Index(frame)] == 0)) {
        ++frame;
      }
      if (frame > stop_frame) {
        break;
      }
      BadInterval b;
      b.start_frame = frame;
      while ((frame <= stop_frame) && (smeared[Index(frame)] != 0)) {
        ++frame;
      }
      if (frame <= stop_frame) {
        b.stop_frame = frame;
        if (b.stop_frame - b.start_frame >= kMinBadIntervalFrames) {
          intervals.push_back(b);
        }
      }
    }
    const int search_range = kBadSearchRangeFrames * kNf;
    for (BadInterval& b : intervals) {
      b.start_sample = (b.start_frame * kHop) + kSb;
      b.stop_sample = (b.stop_frame * kHop) + kNf + kSb;
      b.stop_frame = std::min(b.stop_frame, stop_frame);
      const int length = b.stop_sample - b.start_sample;
      std::vector<float> r(Index((2 * search_range) + length), 0.0F);
      std::vector<float> d(r.size(), 0.0F);
      for (int i = 0; i < length; ++i) {
        r[Index(search_range + i)] = ref.data[Index(b.start_sample + i)];
      }
      const int limit = max_samples - kSb + kPad;
      for (int i = 0; std::cmp_less(i, d.size()); ++i) {
        int j = b.start_sample - search_range + i;
        j = std::max(j, kSb);
        j = std::min(j, limit - 1);
        d[Index(i)] = tweaked_deg[Index(j)];
      }
      const DelayEstimate e =
          ComputeDelay(0, static_cast<int>(r.size()), search_range, r, d);
      b.delay = e.correlation < kMinBadCorrelation ? 0 : e.delay;
    }
    if (!intervals.empty()) {
      std::vector<float> doubly_tweaked(tweaked_deg.begin(),
                                        tweaked_deg.begin() + nn);
      for (const BadInterval& b : intervals) {
        for (int i = b.start_sample; i < b.stop_sample; ++i) {
          const int j = std::clamp(i + b.delay, 0, max_samples - 1);
          doubly_tweaked[Index(i)] = tweaked_deg[Index(j)];
        }
      }
      for (const BadInterval& b : intervals) {
        for (int f = b.start_frame; f < b.stop_frame; ++f) {
          const int start = kSb + (f * kHop);
          ShortTermSpectrum(doubly_tweaked, start, window, spectrum_deg, fft);
          FrequencyWarp(spectrum_deg, FrameOf(density_deg, f));
        }
        CompensateGain(b.start_frame, b.stop_frame, density_ref, density_deg,
                       [&](int f, float /*audible_ref*/) {
                         const FrameDisturbance fd = DisturbanceOf(
                             FrameOf(density_ref, f), FrameOf(density_deg, f));
                         frame_disturbance[Index(f)] = std::min(
                             frame_disturbance[Index(f)], fd.symmetric);
                         frame_disturbance_asym[Index(f)] = std::min(
                             frame_disturbance_asym[Index(f)], fd.asymmetric);
                       });
      }
    }
  }

  // ITU-T P.862 §10.2.15: the recency weighting of long files.
  std::vector<float> time_weight(Index(frames), 1.0F);
  if (stop_frame + 1 > 1000) {
    const int n = ((max_samples - (2 * kSb)) / kHop) - 1;
    double factor = (static_cast<float>(n) - 1000.0F) / 5500.0F;
    if (factor > 0.5F) {
      factor = 0.5F;
    }
    for (int f = 0; f <= stop_frame; ++f) {
      time_weight[Index(f)] =
          static_cast<float>((1.0F - factor) + factor * static_cast<float>(f) /
                                                   static_cast<float>(n));
    }
  }

  // ITU-T P.862 §10.2.11: emphasis on soft frames of the reference.
  for (int f = 0; f <= stop_frame; ++f) {
    const auto h = static_cast<float>(
        std::pow((total_power_ref[Index(f)] + 1E5) / 1E7, 0.04));
    frame_disturbance[Index(f)] /= h;
    frame_disturbance_asym[Index(f)] /= h;
    frame_disturbance[Index(f)] =
        std::min(frame_disturbance[Index(f)], kMaxFrameDisturbance);
    frame_disturbance_asym[Index(f)] =
        std::min(frame_disturbance_asym[Index(f)], kMaxFrameDisturbance);
  }

  const float d_indicator =
      LpqWeight(start_frame, stop_frame, t::kDisturbancePowerSyllable,
                t::kDisturbancePowerTime, frame_disturbance, time_weight);
  const float a_indicator =
      LpqWeight(start_frame, stop_frame, t::kAsymmetryPowerSyllable,
                t::kAsymmetryPowerTime, frame_disturbance_asym, time_weight);
  // ITU-T P.862 §10.2.16.
  return static_cast<float>(4.5 - t::kDisturbanceWeight * d_indicator -
                            t::kAsymmetryWeight * a_indicator);
}

}  // namespace speak5220::core::quality::pesq
