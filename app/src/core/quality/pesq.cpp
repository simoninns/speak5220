/*
 * File:        pesq.cpp
 * Module:      quality
 * Purpose:     ITU-T P.862 narrowband PESQ at 8 kHz, mapped to MOS-LQO by
 *              ITU-T P.862.1
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/pesq.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <utility>
#include <vector>

#include "core/quality/pesq_stages.h"
#include "core/quality/pesq_tables.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

namespace t = pesq_tables;

namespace {

// The signal divided by `peak`, in single precision.
std::vector<float> Normalised(std::span<const double> x, double peak) {
  std::vector<float> out(x.size());
  std::ranges::transform(x, out.begin(), [peak](double v) {
    return static_cast<float>(v / peak);
  });
  return out;
}

}  // namespace

float MosLqoFromRaw(float raw) {
  // ITU-T P.862.1: MOS-LQO = 0.999 + 4 / (1 + exp(-1.4945 x + 4.6607)).
  return 0.999F +
         4.0F / (1.0F + static_cast<float>(std::exp(
                            static_cast<double>(-1.4945F * raw + 4.6607F))));
}

StatusOr<PesqScore> ScorePesq(std::span<const double> reference,
                              std::span<const double> degraded) {
  if (std::cmp_less(reference.size(), kPesqMinSamples) ||
      std::cmp_less(degraded.size(), kPesqMinSamples)) {
    return InvalidArgumentError("PESQ needs at least a quarter of a second");
  }
  double peak = 0.0;
  for (std::span<const double> x : {reference, degraded}) {
    for (double v : x) {
      if (!std::isfinite(v)) {
        return InvalidArgumentError("PESQ input is not finite");
      }
      peak = std::max(peak, std::fabs(v));
    }
  }
  if (peak == 0.0) {
    return InvalidArgumentError("PESQ input is silent");
  }

  // ITU-T P.862 Figure 3 and §10.1: level alignment, IRS filtering, then
  // time alignment on a copy given the input filter.
  pesq::Signal ref = pesq::MakeSignal(Normalised(reference, peak));
  pesq::Signal deg = pesq::MakeSignal(Normalised(degraded, peak));
  pesq::Scratch scratch = pesq::MakeScratch(ref, deg);
  const int max_samples = std::max(ref.samples, deg.samples);
  pesq::FixPowerLevel(ref, max_samples);
  pesq::FixPowerLevel(deg, max_samples);
  pesq::ApplyFilterCurve(ref.data, ref.samples, t::kIrsReceiveCurve);
  pesq::ApplyFilterCurve(deg.data, deg.samples, t::kIrsReceiveCurve);
  std::vector<float> model_ref = ref.data;
  std::vector<float> model_deg = deg.data;
  for (pesq::Signal* s : {&ref, &deg}) {
    pesq::DcBlock(s->data, s->samples);
    pesq::ApplyInputFilter(s->data, s->samples + t::kDataPaddingSamples);
    pesq::ComputeVoiceActivity(*s);
  }
  pesq::Alignment alignment;
  pesq::CrudeAlign(ref, deg, alignment, pesq::kWholeSignal, scratch);
  if (Status s = pesq::LocateUtterances(ref, deg, alignment, scratch);
      !s.IsOk()) {
    return s;
  }

  // ITU-T P.862 §10.2: the perceptual model works on the IRS-filtered
  // signals, the shorter one padded with zeros (its sample count is kept,
  // as the reference keeps it).
  ref.data = std::move(model_ref);
  deg.data = std::move(model_deg);
  const auto padded =
      static_cast<std::size_t>(max_samples) + t::kDataPaddingSamples;
  ref.data.resize(padded, 0.0F);
  deg.data.resize(padded, 0.0F);
  StatusOr<float> raw = pesq::PerceptualModel(ref, deg, alignment);
  if (!raw.IsOk()) {
    return raw.GetStatus();
  }
  PesqScore score;
  score.raw = *raw;
  score.mos_lqo = MosLqoFromRaw(score.raw);
  Logger(kLogModuleQuality)
      ->debug("PESQ: {} utterances, raw {:.3f}, MOS-LQO {:.3f}",
              alignment.utterances, score.raw, score.mos_lqo);
  return score;
}

}  // namespace speak5220::core::quality
