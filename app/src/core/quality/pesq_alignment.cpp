/*
 * File:        pesq_alignment.cpp
 * Module:      quality
 * Purpose:     PESQ's level alignment, IRS filtering, voice activity and
 *              time alignment (ITU-T P.862 §10.1)
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
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

// The reference's 2π for its window functions, a double literal.
constexpr double kTwoPi = 6.28318530717959;

// The fine-alignment histogram is smoothed by a triangle this many
// samples wide on each side: ITU-T P.862 §10.1.3.2, 1 ms at 8 kHz.
constexpr int kKernel = t::kAlignFftSize / 64;

// Utterances shorter than this (envelope frames of speech) are not
// tested for a delay change within them.
constexpr int kMinSplitLength = 200;

// The most break points tried in one utterance split.
constexpr int kMaxBreakPoints = 40;

std::size_t Index(int i) { return static_cast<std::size_t>(i); }

// The Hann window of the fine alignment, in float as the reference holds
// it.
float AlignWindow(int i) {
  return static_cast<float>(0.5 *
                            (1.0 - std::cos((kTwoPi * i) / t::kAlignFftSize)));
}

// One second-order section over x[0, n), with the reference's shortcuts
// for zero coefficients: they skip terms, which can change the rounding.
void FilterSection(std::span<float> x, int n, const t::Biquad& h) {
  float z1 = 0.0F;
  float z2 = 0.0F;
  const bool recursive = (h.a1 != 0.0F) || (h.a2 != 0.0F);
  const bool feedforward = (h.b1 != 0.0F) || (h.b2 != 0.0F);
  for (std::size_t i = 0; i < Index(n); ++i) {
    if (recursive) {
      const float z0 = x[i] - h.a1 * z1 - h.a2 * z2;
      if (feedforward) {
        x[i] = h.b0 * z0 + h.b1 * z1 + h.b2 * z2;
      } else if (h.b0 != 1.0F) {
        x[i] = h.b0 * z0;
      } else {
        x[i] = z0;
      }
      z2 = z1;
      z1 = z0;
    } else if (feedforward) {
      const float z0 = x[i];
      x[i] = h.b0 * z0 + h.b1 * z1 + h.b2 * z2;
      z2 = z1;
      z1 = z0;
    } else if (h.b0 != 1.0F) {
      x[i] = h.b0 * x[i];
    }
  }
}

// The cross-spectrum of two windowed frames, back in the time domain:
// X1 = IFFT(conj(FFT(x1)) FFT(x2)), in place in `x1`.
void FrameCrossCorrelation(std::span<float> x1, std::span<float> x2) {
  constexpr int kN = t::kAlignFftSize;
  RealFft(x1, kN);
  RealFft(x2, kN);
  for (std::size_t c = 0; c <= kN / 2; ++c) {
    const float r1 = x1[c * 2];
    const float i1 = -x1[1 + (c * 2)];
    x1[c * 2] = (r1 * x2[c * 2] - i1 * x2[1 + (c * 2)]);
    x1[1 + (c * 2)] = (r1 * x2[1 + (c * 2)] + i1 * x2[c * 2]);
  }
  RealIfft(x1, kN);
}

// Replaces the correlation in x1[0, kAlignFftSize) by its magnitude and
// returns 0.99 times its peak: the frame's peak threshold.
float PeakThreshold(std::span<float> x1) {
  float v_max = 0.0F;
  for (std::size_t c = 0; c < t::kAlignFftSize; ++c) {
    const float r1 = std::fabs(x1[c]);
    x1[c] = r1;
    if (r1 > v_max) {
      v_max = r1;
    }
  }
  return v_max * 0.99F;
}

// The histogram's peak: its index as a signed delay, and its height.
struct HistogramPeak {
  int delay = 0;
  float height = 0.0F;
};

HistogramPeak PeakOf(std::span<const float> h) {
  HistogramPeak peak;
  int i_max = 0;
  for (int c = 0; c < t::kAlignFftSize; ++c) {
    if (h[Index(c)] > peak.height) {
      peak.height = h[Index(c)];
      i_max = c;
    }
  }
  if (i_max >= t::kAlignFftSize / 2) {
    i_max -= t::kAlignFftSize;
  }
  peak.delay = i_max;
  return peak;
}

// Adds one frame pair to a split-alignment histogram: each correlation
// peak above the frame's threshold adds a triangle of weight
// peak^0.125 / kKernel per step, smoothed as it is added.
void AddSmoothedFrame(std::span<float> x1, std::span<float> h, float& h_sum) {
  const float v_max = PeakThreshold(x1);
  const float n_max =
      static_cast<float>(std::pow(static_cast<double>(v_max), 0.125)) /
      static_cast<float>(kKernel);
  for (int count = 0; count < t::kAlignFftSize; ++count) {
    if (x1[Index(count)] > v_max) {
      h_sum += n_max * static_cast<float>(kKernel);
      for (int k = 1 - kKernel; k < kKernel; ++k) {
        h[Index((count + k + t::kAlignFftSize) % t::kAlignFftSize)] +=
            n_max *
            (static_cast<float>(kKernel) - static_cast<float>(std::abs(k)));
      }
    }
  }
}

// The scratch layout of the split search (the reference's split_align()).
struct SplitBuffers {
  std::span<float> x1;
  std::span<float> x2;
  std::span<float> h;
  std::span<float> window;
};

SplitBuffers SplitBuffersOf(Scratch& scratch) {
  constexpr std::size_t kN = t::kAlignFftSize;
  std::span<float> s(scratch);
  return {.x1 = s.subspan(0, kN + 2),
          .x2 = s.subspan(kN + 2, kN + 2),
          .h = s.subspan(4 + (2 * kN), kN),
          .window = s.subspan(6 + (3 * kN), kN)};
}

// Fills x1 and x2 with windowed frames of the two signals.
void WindowedFrames(const SplitBuffers& b, const Signal& ref, int start_ref,
                    const Signal& deg, int start_deg) {
  for (std::size_t c = 0; c < t::kAlignFftSize; ++c) {
    b.x1[c] = ref.data[Index(start_ref) + c] * b.window[c];
    b.x2[c] = deg.data[Index(start_deg) + c] * b.window[c];
  }
}

// One utterance-split candidate: the crude and fine delays and the
// confidence of the part before (1) or after (2) the break point.
struct SplitCandidates {
  std::array<int, kMaxBreakPoints + 1> break_point{};
  std::array<int, kMaxBreakPoints + 1> estimate1{};
  std::array<int, kMaxBreakPoints + 1> estimate2{};
  std::array<int, kMaxBreakPoints + 1> delay1{};
  std::array<int, kMaxBreakPoints + 1> delay2{};
  std::array<float, kMaxBreakPoints + 1> confidence1{};
  std::array<float, kMaxBreakPoints + 1> confidence2{};
};

// The best split of an utterance found by SplitAlign().
struct BestSplit {
  int estimate1 = 0;
  int delay1 = 0;
  float confidence1 = 0.0F;
  int estimate2 = 0;
  int delay2 = 0;
  float confidence2 = 0.0F;
  int break_point = 0;
};

// The span of one utterance under test for a split.
struct UtteranceSpan {
  int start = 0;
  int speech_start = 0;
  int speech_end = 0;
  int end = 0;
  int delay_estimate = 0;
  float delay_confidence = 0.0F;
};

// ITU-T P.862 §10.1.3.3: tries break points through an utterance and
// aligns the parts before and after each one (the reference's
// split_align()). Frames before a break point are accumulated from the
// utterance's start, and the histograms of break points with the same
// crude delay share the frames already added, as in the reference.
BestSplit SplitAlign(const Signal& ref, const Signal& deg, Alignment& alignment,
                     Scratch& scratch, const UtteranceSpan& u) {
  constexpr int kN = t::kAlignFftSize;
  constexpr int kDs = t::kDownsample;
  constexpr int kTest = kSplitTest - 1;
  BestSplit best;
  const SplitBuffers b = SplitBuffersOf(scratch);
  for (std::size_t c = 0; c < kN; ++c) {
    b.window[c] = AlignWindow(static_cast<int>(c));
  }
  const int utt_len = u.speech_end - u.speech_start;
  const int delta = kN / (4 * kDs);
  int step =
      static_cast<int>((0.801 * utt_len + 40 * delta - 1) / (40 * delta));
  step *= delta;
  const int pad = std::max(utt_len / 10, 75);

  SplitCandidates s;
  s.break_point[0] = u.speech_start + pad;
  int n_bps = 0;
  while (true) {
    ++n_bps;
    s.break_point[Index(n_bps)] = s.break_point[Index(n_bps - 1)] + step;
    if ((s.break_point[Index(n_bps)] > (u.speech_end - pad)) ||
        (n_bps >= kMaxBreakPoints)) {
      break;
    }
  }

  for (int bp = 0; bp < n_bps; ++bp) {
    const auto i = Index(bp);
    alignment.delay_estimate[kTest] = u.delay_estimate;
    alignment.search_start[kTest] = u.start;
    alignment.search_end[kTest] = s.break_point[i];
    CrudeAlign(ref, deg, alignment, kSplitTest, scratch);
    s.estimate1[i] = alignment.delay[kTest];
    alignment.delay_estimate[kTest] = u.delay_estimate;
    alignment.search_start[kTest] = s.break_point[i];
    alignment.search_end[kTest] = u.end;
    CrudeAlign(ref, deg, alignment, kSplitTest, scratch);
    s.estimate2[i] = alignment.delay[kTest];
  }

  // The parts before each break point, accumulated forwards.
  constexpr float kPending = -2.0F;
  std::fill_n(s.confidence1.begin(), n_bps, kPending);
  while (true) {
    int bp = 0;
    while ((bp < n_bps) && (s.confidence1[Index(bp)] > kPending)) {
      ++bp;
    }
    if (bp >= n_bps) {
      break;
    }
    const int estimate = s.estimate1[Index(bp)];
    std::ranges::fill(b.h, 0.0F);
    float h_sum = 0.0F;
    int start_ref = u.start * kDs;
    int start_deg = start_ref + estimate;
    if (start_deg < 0) {
      start_ref = -estimate;
      start_deg = 0;
    }
    auto accumulate_to = [&](int break_point) {
      while (((start_deg + kN) <= deg.samples) &&
             ((start_ref + kN) <= (break_point * kDs))) {
        WindowedFrames(b, ref, start_ref, deg, start_deg);
        FrameCrossCorrelation(b.x1, b.x2);
        AddSmoothedFrame(b.x1, b.h, h_sum);
        start_ref += kN / 4;
        start_deg += kN / 4;
      }
      const HistogramPeak peak = PeakOf(b.h);
      s.delay1[Index(bp)] = estimate + peak.delay;
      s.confidence1[Index(bp)] = h_sum > 0.0 ? peak.height / h_sum : 0.0F;
    };
    accumulate_to(s.break_point[Index(bp)]);
    while (bp < (n_bps - 1)) {
      ++bp;
      if ((s.estimate1[Index(bp)] == estimate) &&
          (s.confidence1[Index(bp)] <= kPending)) {
        accumulate_to(s.break_point[Index(bp)]);
      }
    }
  }

  // The parts after each break point, accumulated backwards from the
  // utterance's end.
  for (int bp = 0; bp < n_bps; ++bp) {
    s.confidence2[Index(bp)] =
        s.confidence1[Index(bp)] > u.delay_confidence ? kPending : 0.0F;
  }
  while (true) {
    int bp = n_bps - 1;
    while ((bp >= 0) && (s.confidence2[Index(bp)] > kPending)) {
      --bp;
    }
    if (bp < 0) {
      break;
    }
    const int estimate = s.estimate2[Index(bp)];
    std::ranges::fill(b.h, 0.0F);
    float h_sum = 0.0F;
    int start_ref = (u.end * kDs) - kN;
    int start_deg = start_ref + estimate;
    if ((start_deg + kN) > deg.samples) {
      start_deg = deg.samples - kN;
      start_ref = start_deg - estimate;
    }
    auto accumulate_to = [&](int break_point) {
      while ((start_deg >= 0) && (start_ref >= (break_point * kDs))) {
        WindowedFrames(b, ref, start_ref, deg, start_deg);
        FrameCrossCorrelation(b.x1, b.x2);
        AddSmoothedFrame(b.x1, b.h, h_sum);
        start_ref -= kN / 4;
        start_deg -= kN / 4;
      }
      const HistogramPeak peak = PeakOf(b.h);
      s.delay2[Index(bp)] = estimate + peak.delay;
      s.confidence2[Index(bp)] = h_sum > 0.0 ? peak.height / h_sum : 0.0F;
    };
    accumulate_to(s.break_point[Index(bp)]);
    while (bp > 0) {
      --bp;
      if ((s.estimate2[Index(bp)] == estimate) &&
          (s.confidence2[Index(bp)] <= kPending)) {
        accumulate_to(s.break_point[Index(bp)]);
      }
    }
  }

  for (int bp = 0; bp < n_bps; ++bp) {
    const auto i = Index(bp);
    if ((std::abs(s.delay2[i] - s.delay1[i]) >= kDs) &&
        ((s.confidence1[i] + s.confidence2[i]) >
         (best.confidence1 + best.confidence2)) &&
        (s.confidence1[i] > u.delay_confidence) &&
        (s.confidence2[i] > u.delay_confidence)) {
      best = {.estimate1 = s.estimate1[i],
              .delay1 = s.delay1[i],
              .confidence1 = s.confidence1[i],
              .estimate2 = s.estimate2[i],
              .delay2 = s.delay2[i],
              .confidence2 = s.confidence2[i],
              .break_point = s.break_point[i]};
    }
  }
  return best;
}

// The search windows: each speech burst of the reference's voice activity,
// widened by kSearchBuffer frames, that is long enough and overlaps the
// degraded signal at the crude delay (the reference's id_searchwindows()).
void FindSearchWindows(const Signal& ref, const Signal& deg,
                       Alignment& alignment) {
  const int vad_length = ref.samples / t::kDownsample;
  const int del_deg_start =
      t::kMinUtteranceLength - (alignment.crude_delay / t::kDownsample);
  const int del_deg_end =
      ((deg.samples - alignment.crude_delay) / t::kDownsample) -
      t::kMinUtteranceLength;
  int utt = 0;
  int this_start = 0;
  bool speech = false;
  for (int count = 0; count < vad_length && utt < t::kMaxUtterances; ++count) {
    const float vad = ref.vad[Index(count)];
    if ((vad > 0.0F) && !speech) {
      speech = true;
      this_start = count;
      alignment.search_start[Index(utt)] =
          std::max(count - t::kSearchBuffer, 0);
    }
    if (((vad == 0.0F) || (count == (vad_length - 1))) && speech) {
      speech = false;
      alignment.search_end[Index(utt)] =
          std::min(count + t::kSearchBuffer, vad_length - 1);
      if (((count - this_start) >= t::kMinUtteranceLength) &&
          (this_start < del_deg_end) && (count > del_deg_start)) {
        ++utt;
      }
    }
  }
  alignment.utterances = utt;
}

// Each utterance's start and end, meeting half way between bursts and
// kept inside the degraded signal at each utterance's delay (the
// reference's id_utterances()).
void FindUtterances(const Signal& ref, const Signal& deg,
                    Alignment& alignment) {
  constexpr int kDs = t::kDownsample;
  const int vad_length = ref.samples / kDs;
  const int del_deg_start =
      t::kMinUtteranceLength - (alignment.crude_delay / kDs);
  const int del_deg_end =
      ((deg.samples - alignment.crude_delay) / kDs) - t::kMinUtteranceLength;
  const int n = alignment.utterances;
  int utt = 0;
  int this_start = 0;
  bool speech = false;
  for (int count = 0; count < vad_length && utt < t::kMaxUtterances; ++count) {
    const float vad = ref.vad[Index(count)];
    if ((vad > 0.0F) && !speech) {
      speech = true;
      this_start = count;
      alignment.start[Index(utt)] = count;
    }
    if (((vad == 0.0F) || (count == (vad_length - 1))) && speech) {
      speech = false;
      alignment.end[Index(utt)] = count;
      if (((count - this_start) >= t::kMinUtteranceLength) &&
          (this_start < del_deg_end) && (count > del_deg_start)) {
        ++utt;
      }
    }
  }
  alignment.start[0] = t::kSearchBuffer;
  alignment.end[Index(n - 1)] = vad_length - t::kSearchBuffer;
  for (int u = 1; u < n; ++u) {
    const int mid =
        (alignment.start[Index(u)] + alignment.end[Index(u - 1)]) / 2;
    alignment.start[Index(u)] = mid;
    alignment.end[Index(u - 1)] = mid;
  }
  if ((alignment.start[0] * kDs) + alignment.delay[0] <
      t::kSearchBufferSamples) {
    alignment.start[0] =
        t::kSearchBuffer + ((kDs - 1 - alignment.delay[0]) / kDs);
  }
  const int last_delay = alignment.delay[Index(n - 1)];
  if ((alignment.end[Index(n - 1)] * kDs) + last_delay >
      (deg.samples - t::kSearchBufferSamples)) {
    alignment.end[Index(n - 1)] =
        ((deg.samples - last_delay) / kDs) - t::kSearchBuffer;
  }
  for (int u = 1; u < n; ++u) {
    const int start =
        (alignment.start[Index(u)] * kDs) + alignment.delay[Index(u)];
    const int last_end =
        (alignment.end[Index(u - 1)] * kDs) + alignment.delay[Index(u - 1)];
    if (start < last_end) {
      const int mid = (start + last_end) / 2;
      alignment.start[Index(u)] =
          (kDs - 1 + mid - alignment.delay[Index(u)]) / kDs;
      alignment.end[Index(u - 1)] = (mid - alignment.delay[Index(u - 1)]) / kDs;
    }
  }
  alignment.largest_utterance = 0;
  for (int u = 0; u < n; ++u) {
    alignment.largest_utterance =
        std::max(alignment.largest_utterance,
                 alignment.end[Index(u)] - alignment.start[Index(u)]);
  }
}

// ITU-T P.862 §10.1.3.3: splits each utterance in two where that aligns
// better, testing the parts again (the reference's utterance_split()).
void SplitUtterances(const Signal& ref, const Signal& deg, Alignment& alignment,
                     Scratch& scratch) {
  constexpr int kDs = t::kDownsample;
  int id = 0;
  while ((id < alignment.utterances) &&
         (alignment.utterances < t::kMaxUtterances)) {
    UtteranceSpan u;
    u.delay_estimate = alignment.delay_estimate[Index(id)];
    u.delay_confidence = alignment.delay_confidence[Index(id)];
    u.start = alignment.start[Index(id)];
    u.end = alignment.end[Index(id)];
    u.speech_start = u.start;
    while ((u.speech_start < u.end) &&
           (ref.vad[Index(u.speech_start)] <= 0.0F)) {
      ++u.speech_start;
    }
    u.speech_end = u.end;
    while ((u.speech_end > u.start) && (ref.vad[Index(u.speech_end)] <= 0.0F)) {
      --u.speech_end;
    }
    ++u.speech_end;
    if (u.speech_end - u.speech_start < kMinSplitLength) {
      ++id;
      continue;
    }
    const BestSplit best = SplitAlign(ref, deg, alignment, scratch, u);
    if ((best.confidence1 <= u.delay_confidence) ||
        (best.confidence2 <= u.delay_confidence)) {
      ++id;
      continue;
    }
    for (int step = alignment.utterances - 1; step > id; --step) {
      const auto to = Index(step + 1);
      const auto from = Index(step);
      alignment.delay_estimate[to] = alignment.delay_estimate[from];
      alignment.delay[to] = alignment.delay[from];
      alignment.delay_confidence[to] = alignment.delay_confidence[from];
      alignment.start[to] = alignment.start[from];
      alignment.end[to] = alignment.end[from];
      // As the reference: the search window takes the utterance's bounds.
      alignment.search_start[to] = alignment.start[from];
      alignment.search_end[to] = alignment.end[from];
    }
    ++alignment.utterances;
    const auto a = Index(id);
    const auto b = Index(id + 1);
    alignment.delay_estimate[a] = best.estimate1;
    alignment.delay[a] = best.delay1;
    alignment.delay_confidence[a] = best.confidence1;
    alignment.delay_estimate[b] = best.estimate2;
    alignment.delay[b] = best.delay2;
    alignment.delay_confidence[b] = best.confidence2;
    alignment.search_start[b] = alignment.search_start[a];
    alignment.search_end[b] = alignment.search_end[a];
    alignment.start[a] = u.start;
    alignment.end[b] = u.end;
    if (best.delay2 < best.delay1) {
      alignment.end[a] = best.break_point;
      alignment.start[b] = best.break_point;
    } else {
      const int half_jump = (best.delay2 - best.delay1) / (2 * kDs);
      alignment.end[a] = best.break_point + half_jump;
      alignment.start[b] = best.break_point - half_jump;
    }
    if (((alignment.start[a] - t::kSearchBuffer) * kDs) + best.delay1 < 0) {
      alignment.start[a] = t::kSearchBuffer + ((kDs - 1 - best.delay1) / kDs);
    }
    if ((alignment.end[b] * kDs) + best.delay2 >
        (deg.samples - t::kSearchBufferSamples)) {
      alignment.end[b] = ((deg.samples - best.delay2) / kDs) - t::kSearchBuffer;
    }
  }
  alignment.largest_utterance = 0;
  for (int u = 0; u < alignment.utterances; ++u) {
    alignment.largest_utterance =
        std::max(alignment.largest_utterance,
                 alignment.end[Index(u)] - alignment.start[Index(u)]);
  }
}

}  // namespace

Signal MakeSignal(std::span<const float> samples) {
  Signal s;
  s.samples = static_cast<int>(samples.size()) + (2 * t::kSearchBufferSamples);
  s.data.assign(Index(s.samples + t::kDataPaddingSamples), 0.0F);
  std::ranges::copy(samples, s.data.begin() + t::kSearchBufferSamples);
  s.vad.assign(Index(s.samples / t::kDownsample), 0.0F);
  s.log_vad.assign(Index(s.samples / t::kDownsample), 0.0F);
  return s;
}

Scratch MakeScratch(const Signal& ref, const Signal& deg) {
  const int size =
      std::max(std::max(ref.samples, deg.samples) + t::kDataPaddingSamples,
               12 * t::kAlignFftSize);
  Scratch scratch(Index(size), 0.0F);
  return scratch;
}

float InterpolateCurve(float hz,
                       std::span<const pesq_tables::CurvePoint> curve) {
  const double f = hz;
  std::size_t i = 1;
  if (f <= curve.front().hz) {
    i = 1;
  } else if (f >= curve.back().hz) {
    i = curve.size() - 1;
  } else {
    while (curve[i].hz < f) {
      ++i;
    }
  }
  const t::CurvePoint& low = curve[i - 1];
  const t::CurvePoint& high = curve[i];
  return static_cast<float>(((f - low.hz) * high.db + (high.hz - f) * low.db) /
                            (high.hz - low.hz));
}

void ApplyFilterCurve(std::span<float> data, int samples,
                      std::span<const pesq_tables::CurvePoint> curve) {
  const int n =
      samples - (2 * t::kSearchBufferSamples) + t::kDataPaddingSamples;
  const int pow2 = NextPowerOfTwo(n);
  std::vector<float> x(Index(pow2 + 2), 0.0F);
  std::copy_n(data.begin() + t::kSearchBufferSamples, n, x.begin());
  RealFft(x, pow2);
  const float overall_gain = InterpolateCurve(1000.0F, curve);
  const float resolution =
      static_cast<float>(t::kSampleRate) / static_cast<float>(pow2);
  for (int i = 0; i <= pow2 / 2; ++i) {
    const float gain_db =
        InterpolateCurve(static_cast<float>(i) * resolution, curve) -
        overall_gain;
    const auto factor = static_cast<float>(
        std::pow(10.0, static_cast<double>(gain_db / 20.0F)));
    x[Index(2 * i)] *= factor;
    x[Index((2 * i) + 1)] *= factor;
  }
  RealIfft(x, pow2);
  std::copy_n(x.begin(), n, data.begin() + t::kSearchBufferSamples);
}

double PowerOf(std::span<const float> x, int start, int stop, int divisor) {
  double power = 0;
  for (int i = start; i < stop; ++i) {
    const float h = x[Index(i)];
    power += h * h;
  }
  return power / divisor;
}

void FixPowerLevel(Signal& signal, int max_samples) {
  std::vector<float> filtered = signal.data;
  ApplyFilterCurve(filtered, signal.samples, t::kLevelAlignCurve);
  const auto power = static_cast<float>(PowerOf(
      filtered, t::kSearchBufferSamples,
      signal.samples - t::kSearchBufferSamples + t::kDataPaddingSamples,
      max_samples - (2 * t::kSearchBufferSamples) + t::kDataPaddingSamples));
  const auto scale = static_cast<float>(
      std::sqrt(t::kTargetAveragePower / static_cast<double>(power)));
  for (std::size_t i = 0; i < Index(signal.samples); ++i) {
    signal.data[i] *= scale;
  }
}

void DcBlock(std::span<float> data, int samples) {
  constexpr int kOfs = t::kSearchBufferSamples;
  float mean = 0.0F;
  for (int i = kOfs; i < samples - kOfs; ++i) {
    mean += data[Index(i)];
  }
  // As the reference: the sum is divided by the padded length.
  mean /= static_cast<float>(samples);
  for (int i = kOfs; i < samples - kOfs; ++i) {
    data[Index(i)] -= mean;
  }
  for (int count = 0; count < t::kDownsample; ++count) {
    const float ramp =
        (0.5F + static_cast<float>(count)) / static_cast<float>(t::kDownsample);
    data[Index(kOfs + count)] *= ramp;
    data[Index(samples - kOfs - 1 - count)] *= ramp;
  }
}

void ApplyInputFilter(std::span<float> data, int length) {
  for (const t::Biquad& section : t::kInputFilter) {
    FilterSection(data, length, section);
  }
}

void ComputeVoiceActivity(Signal& signal) {
  constexpr int kDs = t::kDownsample;
  const int n_windows = signal.samples / kDs;
  std::vector<float>& vad = signal.vad;
  for (int w = 0; w < n_windows; ++w) {
    float e = 0.0F;
    for (int i = 0; i < kDs; ++i) {
      const float g = signal.data[Index((w * kDs) + i)];
      e += (g * g);
    }
    vad[Index(w)] = e / static_cast<float>(kDs);
  }
  const auto n_float = static_cast<float>(n_windows);
  float level_thresh = 0.0F;
  for (int w = 0; w < n_windows; ++w) {
    level_thresh += vad[Index(w)];
  }
  level_thresh /= n_float;
  float level_min = 0.0F;
  for (int w = 0; w < n_windows; ++w) {
    level_min = std::max(level_min, vad[Index(w)]);
  }
  level_min = level_min > 0.0F ? level_min * 1.0e-4F : 1.0F;
  for (int w = 0; w < n_windows; ++w) {
    vad[Index(w)] = std::max(vad[Index(w)], level_min);
  }
  // The noise level and its deviation, below the threshold, refined.
  constexpr int kIterations = 12;
  for (int iteration = 0; iteration < kIterations; ++iteration) {
    float level_noise = 0.0F;
    float std_noise = 0.0F;
    int length = 0;
    for (int w = 0; w < n_windows; ++w) {
      if (vad[Index(w)] <= level_thresh) {
        level_noise += vad[Index(w)];
        ++length;
      }
    }
    if (length > 0) {
      level_noise /= static_cast<float>(length);
      for (int w = 0; w < n_windows; ++w) {
        if (vad[Index(w)] <= level_thresh) {
          const float g = vad[Index(w)] - level_noise;
          std_noise += g * g;
        }
      }
      std_noise = static_cast<float>(std::sqrt(
          static_cast<double>(std_noise / static_cast<float>(length))));
    }
    level_thresh = 1.001F * (level_noise + 2.0F * std_noise);
  }
  float level_noise = 0.0F;
  float level_sig = 0.0F;
  int length = 0;
  for (int w = 0; w < n_windows; ++w) {
    if (vad[Index(w)] > level_thresh) {
      level_sig += vad[Index(w)];
      ++length;
    } else {
      level_noise += vad[Index(w)];
    }
  }
  if (length > 0) {
    level_sig /= static_cast<float>(length);
  } else {
    level_thresh = -1.0F;
  }
  if (length < n_windows) {
    level_noise /= static_cast<float>(n_windows - length);
  } else {
    level_noise = 1.0F;
  }
  // Speech frames positive, the rest negative.
  for (int w = 0; w < n_windows; ++w) {
    if (vad[Index(w)] <= level_thresh) {
      vad[Index(w)] = -vad[Index(w)];
    }
  }
  vad[0] = -level_min;
  vad[Index(n_windows - 1)] = -level_min;
  // Drop bursts of kMinSpeechLength frames or fewer.
  int start = 0;
  int finish = 0;
  for (int w = 1; w < n_windows; ++w) {
    if ((vad[Index(w)] > 0.0F) && (vad[Index(w - 1)] <= 0.0F)) {
      start = w;
    }
    if ((vad[Index(w)] <= 0.0F) && (vad[Index(w - 1)] > 0.0F)) {
      finish = w;
      if ((finish - start) <= t::kMinSpeechLength) {
        for (int i = start; i < finish; ++i) {
          vad[Index(i)] = -vad[Index(i)];
        }
      }
    }
  }
  // With a clean signal, drop bursts quieter than three times the
  // threshold.
  if (level_sig >= (level_noise * 1000.0F)) {
    for (int w = 1; w < n_windows; ++w) {
      if ((vad[Index(w)] > 0.0F) && (vad[Index(w - 1)] <= 0.0F)) {
        start = w;
      }
      if ((vad[Index(w)] <= 0.0F) && (vad[Index(w - 1)] > 0.0F)) {
        finish = w;
        float g = 0.0F;
        for (int i = start; i < finish; ++i) {
          g += vad[Index(i)];
        }
        if (g < 3.0F * level_thresh * static_cast<float>(finish - start)) {
          for (int i = start; i < finish; ++i) {
            vad[Index(i)] = -vad[Index(i)];
          }
        }
      }
    }
  }
  // Join bursts separated by kJoinSpeechLength frames or fewer.
  finish = 0;
  for (int w = 1; w < n_windows; ++w) {
    if ((vad[Index(w)] > 0.0F) && (vad[Index(w - 1)] <= 0.0F)) {
      start = w;
      if ((finish > 0) && ((start - finish) <= t::kJoinSpeechLength)) {
        for (int i = finish; i < start; ++i) {
          vad[Index(i)] = level_min;
        }
      }
    }
    if ((vad[Index(w)] <= 0.0F) && (vad[Index(w - 1)] > 0.0F)) {
      finish = w;
    }
  }
  // No burst at all: everything is speech.
  start = 0;
  for (int w = 1; w < n_windows; ++w) {
    if ((vad[Index(w)] > 0.0F) && (vad[Index(w - 1)] <= 0.0F)) {
      start = w;
    }
  }
  if (start == 0) {
    for (int w = 0; w < n_windows; ++w) {
      vad[Index(w)] = std::fabs(vad[Index(w)]);
    }
    vad[0] = -level_min;
    vad[Index(n_windows - 1)] = -level_min;
  }
  // Ramp each burst's edges over two frames.
  int count = 3;
  while (count < (n_windows - 2)) {
    if ((vad[Index(count)] > 0.0F) && (vad[Index(count - 2)] <= 0.0F)) {
      vad[Index(count - 2)] = vad[Index(count)] * 0.1F;
      vad[Index(count - 1)] = vad[Index(count)] * 0.3F;
      ++count;
    }
    if ((vad[Index(count)] <= 0.0F) && (vad[Index(count - 1)] > 0.0F)) {
      vad[Index(count)] = vad[Index(count - 1)] * 0.3F;
      vad[Index(count + 1)] = vad[Index(count - 1)] * 0.1F;
      count += 3;
    }
    ++count;
  }
  for (int w = 0; w < n_windows; ++w) {
    vad[Index(w)] = std::max(vad[Index(w)], 0.0F);
  }
  if (level_thresh <= 0.0F) {
    level_thresh = level_min;
  }
  for (int w = 0; w < n_windows; ++w) {
    signal.log_vad[Index(w)] =
        vad[Index(w)] <= level_thresh
            ? 0.0F
            : static_cast<float>(
                  std::log(static_cast<double>(vad[Index(w)] / level_thresh)));
  }
}

void CrudeAlign(const Signal& ref, const Signal& deg, Alignment& alignment,
                int utterance, Scratch& scratch) {
  constexpr int kDs = t::kDownsample;
  constexpr int kTest = kSplitTest - 1;
  int nr = 0;
  int nd = 0;
  int start_ref = 0;
  int start_deg = 0;
  const int deg_frames = deg.samples / kDs;
  if (utterance == kWholeSignal) {
    nr = ref.samples / kDs;
    nd = deg_frames;
  } else {
    const auto u = Index(utterance == kSplitTest ? kTest : utterance);
    const int estimate = utterance == kSplitTest ? alignment.delay_estimate[u]
                                                 : alignment.crude_delay;
    start_ref = alignment.search_start[u];
    start_deg = start_ref + (estimate / kDs);
    if (start_deg < 0) {
      start_ref = -estimate / kDs;
      start_deg = 0;
    }
    nr = alignment.search_end[u] - start_ref;
    nd = nr;
    if (start_deg + nd > deg_frames) {
      nd = deg_frames - start_deg;
    }
  }
  int i_max = nr - 1;
  if ((nr > 1) && (nd > 1)) {
    const std::vector<float> y = CrossCorrelate(
        std::span(ref.log_vad).subspan(Index(start_ref), Index(nr)),
        std::span(deg.log_vad).subspan(Index(start_deg), Index(nd)));
    // The reference correlates into its scratch buffer.
    std::ranges::copy(y, scratch.begin());
    float max = 0.0F;
    for (int count = 0; std::cmp_less(count, y.size()); ++count) {
      if (y[Index(count)] > max) {
        max = y[Index(count)];
        i_max = count;
      }
    }
  }
  const int delay = (i_max - nr + 1) * kDs;
  if (utterance == kWholeSignal) {
    alignment.crude_delay = delay;
  } else if (utterance == kSplitTest) {
    alignment.delay[kTest] = delay + alignment.delay_estimate[kTest];
  } else {
    alignment.delay_estimate[Index(utterance)] = delay + alignment.crude_delay;
  }
}

void TimeAlign(const Signal& ref, const Signal& deg, Alignment& alignment,
               int utterance, Scratch& scratch) {
  constexpr int kN = t::kAlignFftSize;
  constexpr int kDs = t::kDownsample;
  const auto u = Index(utterance);
  std::span<float> s(scratch);
  std::span<float> x1 = s.subspan(0, kN + 2);
  std::span<float> x2 = s.subspan(kN + 2, kN + 2);
  std::span<float> h = s.subspan(4 + (2 * kN), kN);
  std::span<float> window = s.subspan(Index(5) * kN, kN);
  std::ranges::fill(h, 0.0F);
  for (std::size_t c = 0; c < kN; ++c) {
    window[c] = AlignWindow(static_cast<int>(c));
  }
  const int estimate = alignment.delay_estimate[u];
  int start_ref = alignment.search_start[u] * kDs;
  int start_deg = start_ref + estimate;
  if (start_deg < 0) {
    start_ref = -estimate;
    start_deg = 0;
  }
  while (((start_deg + kN) <= deg.samples) &&
         ((start_ref + kN) <= (alignment.search_end[u] * kDs))) {
    for (std::size_t c = 0; c < kN; ++c) {
      x1[c] = ref.data[Index(start_ref) + c] * window[c];
      x2[c] = deg.data[Index(start_deg) + c] * window[c];
    }
    FrameCrossCorrelation(x1, x2);
    const float v_max = PeakThreshold(x1);
    for (std::size_t c = 0; c < kN; ++c) {
      if (x1[c] > v_max) {
        h[c] += static_cast<float>(std::pow(static_cast<double>(v_max), 0.125));
      }
    }
    start_ref += kN / 4;
    start_deg += kN / 4;
  }
  // Smooth the histogram with a triangle by circular convolution.
  float h_sum = 0.0F;
  for (std::size_t c = 0; c < kN; ++c) {
    h_sum += h[c];
    x1[c] = h[c];
    x2[c] = 0.0F;
  }
  x2[0] = 1.0F;
  for (int c = 1; c < kKernel; ++c) {
    const float w =
        1.0F - (static_cast<float>(c) / static_cast<float>(kKernel));
    x2[Index(c)] = w;
    x2[Index(kN - c)] = w;
  }
  RealFft(x1, kN);
  RealFft(x2, kN);
  for (std::size_t c = 0; c <= kN / 2; ++c) {
    const float r1 = x1[c * 2];
    const float i1 = x1[1 + (c * 2)];
    x1[c * 2] = (r1 * x2[c * 2] - i1 * x2[1 + (c * 2)]);
    x1[1 + (c * 2)] = (r1 * x2[1 + (c * 2)] + i1 * x2[c * 2]);
  }
  RealIfft(x1, kN);
  for (std::size_t c = 0; c < kN; ++c) {
    h[c] = h_sum > 0.0 ? std::fabs(x1[c]) / h_sum : 0.0F;
  }
  const HistogramPeak peak = PeakOf(h);
  alignment.delay[u] = estimate + peak.delay;
  alignment.delay_confidence[u] = peak.height;
}

Status LocateUtterances(const Signal& ref, const Signal& deg,
                        Alignment& alignment, Scratch& scratch) {
  FindSearchWindows(ref, deg, alignment);
  if (alignment.utterances < 1) {
    return FailedPreconditionError("PESQ found no utterances");
  }
  for (int u = 0; u < alignment.utterances; ++u) {
    CrudeAlign(ref, deg, alignment, u, scratch);
    TimeAlign(ref, deg, alignment, u, scratch);
  }
  FindUtterances(ref, deg, alignment);
  SplitUtterances(ref, deg, alignment, scratch);
  return OkStatus();
}

}  // namespace speak5220::core::quality::pesq
