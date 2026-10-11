/*
 * File:        repeat_search.cpp
 * Module:      encoder
 * Purpose:     Quantises each frame's K and chooses silence, full frames
 *              and repeats by dynamic programming over bits and distortion
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/repeat_search.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/lpc_analysis.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/quantiser.h"
#include "core/util/logging.h"

namespace speak5220::core::encoder {
namespace {

// The pitch range the coding table covers (§8.1, Table 8-5).
constexpr double kShortestPeriod = 16.0;
constexpr double kLongestPeriod = 160.0;
// A white-noise floor on the pooled r[0], as for each frame's.
constexpr double kNoiseFloor = 1.0001;
// Prediction errors are floored here before their ratio is taken.
constexpr double kErrorFloor = 1e-30;

// A frame quantised on its own; a gated frame is not sounding.
struct QuantisedFrame {
  bool sounding = false;
  bool voiced = false;
  std::array<double, kAutocorrelationSize> r = {};
  int order = 0;
  std::vector<int> k;
  double error = 0.0;
  int pitch = 0;
};

struct SegmentChoice {
  double cost = 0.0;
  std::vector<int> k;
};

// The cost and K of frames a..b-1 of a run sent as one full frame and
// repeats. Re-implements tms5220enc.py _segment().
SegmentChoice SegmentCost(std::span<const QuantisedFrame> run, std::size_t a,
                          std::size_t b, double lambda) {
  const QuantisedFrame& head = run[a];
  std::vector<std::vector<int>> cands = {head.k};
  if (b - a > 1) {
    std::array<double, kAutocorrelationSize> pooled = {};
    bool first = true;
    for (std::size_t i = a; i < b; ++i) {
      for (std::size_t t = 0; t < kAutocorrelationSize; ++t) {
        const double v = run[i].r.at(t) / run[i].r[0];
        pooled.at(t) = first ? v : pooled.at(t) + v;
      }
      first = false;
    }
    pooled[0] *= kNoiseFloor;
    cands.push_back(
        QuantiseK(dsp::Levinson(pooled, head.order), pooled).indices);
  }
  const int bits = (head.voiced ? kVoicedFrameBits : kUnvoicedFrameBits) +
                   (kRepeatFrameBits * static_cast<int>(b - a - 1));
  SegmentChoice best;
  for (std::size_t c = 0; c < cands.size(); ++c) {
    double d = 0.0;
    for (std::size_t i = a; i < b; ++i) {
      const double err = QuantisedPredictionError(cands[c], run[i].r);
      d += 10.0 * std::log10(std::max(err, kErrorFloor) /
                             std::max(run[i].error, kErrorFloor));
    }
    const double cost = static_cast<double>(bits) + (lambda * d);
    // The first candidate wins a tie.
    if (c == 0 || cost < best.cost) {
      best.cost = cost;
      best.k = std::move(cands[c]);
    }
  }
  return best;
}

// The cheapest split of one run of same-voicing frames into segments of at
// most max_run frames. Re-implements tms5220enc.py _repeatRun().
void PlanRun(std::span<const QuantisedFrame> run, double lambda, int max_run,
             std::vector<PlanEntry>& plan) {
  const std::size_t n = run.size();
  std::vector<double> cost(n + 1, std::numeric_limits<double>::infinity());
  cost[0] = 0.0;
  std::vector<std::size_t> back(n + 1, 0);
  std::vector<std::vector<int>> k_of(n + 1);
  for (std::size_t e = 1; e <= n; ++e) {
    const auto reach = static_cast<std::size_t>(max_run);
    const std::size_t from = e > reach ? e - reach : 0;
    for (std::size_t st = from; st < e; ++st) {
      SegmentChoice choice = SegmentCost(run, st, e, lambda);
      if (cost[st] + choice.cost < cost[e]) {
        cost[e] = cost[st] + choice.cost;
        back[e] = st;
        k_of[e] = std::move(choice.k);
      }
    }
  }
  const std::size_t base = plan.size();
  plan.resize(base + n);
  for (std::size_t e = n; e > 0;) {
    const std::size_t st = back[e];
    for (std::size_t i = st; i < e; ++i) {
      PlanKind kind = PlanKind::kRepeat;
      if (i == st) {
        kind = run[i].voiced ? PlanKind::kVoiced : PlanKind::kUnvoiced;
      }
      plan[base + i] = {.kind = kind, .k = k_of[e], .pitch = run[i].pitch};
    }
    e = st;
  }
}

}  // namespace

std::vector<PlanEntry> RepeatSearch::Plan(
    std::span<const FrameFeatures> features, const EncoderOptions& options) {
  const auto log = Logger(kLogModuleEncoder);
  std::vector<QuantisedFrame> info;
  info.reserve(features.size());
  for (const FrameFeatures& f : features) {
    if (f.gated) {
      info.emplace_back();
      continue;
    }
    QuantisedFrame q;
    q.sounding = true;
    q.voiced = f.voiced;
    q.r = f.voiced ? f.rv : f.ru;
    q.order = f.voiced ? kLpcOrder : kUnvoicedLpcOrder;
    QuantisedK quantised = QuantiseK(dsp::Levinson(q.r, q.order), q.r);
    q.k = std::move(quantised.indices);
    q.error = quantised.error;
    q.pitch = f.voiced
                  ? PitchIndex(std::max(kShortestPeriod,
                                        std::min(kLongestPeriod, f.period)))
                  : 0;
    info.emplace_back(std::move(q));
  }

  const int max_run = options.repeats ? kMaxRepeatRun : 1;
  std::vector<PlanEntry> plan;
  std::vector<QuantisedFrame> run;
  std::size_t j = 0;
  while (j < info.size()) {
    if (!info[j].sounding) {
      plan.push_back({});
      ++j;
      continue;
    }
    const bool voiced = info[j].voiced;
    run.clear();
    while (j < info.size() && info[j].sounding && info[j].voiced == voiced) {
      run.push_back(info[j]);
      ++j;
    }
    PlanRun(run, options.repeat_lambda, max_run, plan);
  }
  while (!plan.empty() && plan.back().kind == PlanKind::kSilence) {
    plan.pop_back();
  }
  log->debug("plan: {} frames", plan.size());
  if (log->should_log(spdlog::level::trace)) {
    for (std::size_t i = 0; i < plan.size(); ++i) {
      log->trace("plan frame {}: kind {}, pitch {}, {} K", i,
                 static_cast<int>(plan[i].kind), plan[i].pitch,
                 plan[i].k.size());
    }
  }
  return plan;
}

}  // namespace speak5220::core::encoder
