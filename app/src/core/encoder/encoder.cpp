/*
 * File:        encoder.cpp
 * Module:      encoder
 * Purpose:     Encodes 8 kHz speech into TMS5220 frames by composing the
 *              analysis, pitch, voicing, repeat and level stages
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/encoder/encoder.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/fft.h"
#include "core/dsp/squelch.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_endings.h"
#include "core/lpc/frame.h"
#include "core/synth/reference_synthesizer.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::core::encoder {
namespace {

// A faint opening is more than this far below the loudest frame.
constexpr double kFaintOpeningDb = -36.0;
// The first pass for a target peak scales by RMS to this, with the peak
// limit out of the way (a lattice value fits in 9 bits plus sign).
constexpr double kPeakPassRms = 20.0;
constexpr double kPeakPassLimit = 511.0;

bool IsVoicedEntry(const PlanEntry& p) {
  return p.kind == PlanKind::kVoiced ||
         (p.kind == PlanKind::kRepeat && p.pitch != 0);
}

int StreamBits(std::span<const lpc::Frame> frames) {
  int bits = 0;
  for (const lpc::Frame& f : frames) {
    bits += lpc::FrameBits(f);
  }
  return bits;
}

// The stages MakeEncoder() owns.
class StandardEncoder : public IEncoder {
 public:
  StandardEncoder()
      : pitch_tracker_(fft_),
        level_setter_(fft_),
        encoder_(analyser_, pitch_tracker_, voicing_classifier_, repeat_search_,
                 level_setter_) {}

  StatusOr<EncodeResult> Encode(std::span<const double> samples,
                                int sample_rate,
                                const EncoderOptions& options) override {
    return encoder_.Encode(samples, sample_rate, options);
  }

 private:
  dsp::PocketFft fft_;
  FrameAnalyser analyser_;
  YinPitchTracker pitch_tracker_;
  VoicingClassifier voicing_classifier_;
  RepeatSearch repeat_search_;
  LevelSetter level_setter_;
  Encoder encoder_;
};

}  // namespace

OpeningTrim TrimFaintOpening(std::vector<PlanEntry>& plan,
                             std::span<const double> levels) {
  const std::size_t n = plan.size();
  std::size_t lead = 0;
  while (lead < n && plan[lead].kind == PlanKind::kSilence) {
    ++lead;
  }
  double loudest = 0.0;
  bool first = true;
  for (std::size_t j = 0; j < n; ++j) {
    if (plan[j].kind != PlanKind::kSilence) {
      loudest = first ? levels[j] : std::max(loudest, levels[j]);
      first = false;
    }
  }
  const double faint = loudest * std::pow(10.0, kFaintOpeningDb / 20.0);
  int dropped = 0;
  while (true) {
    std::size_t end = lead;
    while (end < n && plan[end].kind != PlanKind::kSilence) {
      ++end;
    }
    if (end >= n) {
      break;
    }
    bool voiced = false;
    double peak = 0.0;
    for (std::size_t j = lead; j < end; ++j) {
      voiced = voiced || IsVoicedEntry(plan[j]);
      peak = j == lead ? levels[j] : std::max(peak, levels[j]);
    }
    if (voiced || peak > faint) {
      break;
    }
    std::size_t next = end;
    while (next < n && plan[next].kind == PlanKind::kSilence) {
      ++next;
    }
    if (next >= n) {
      break;
    }
    dropped += static_cast<int>(next - lead);
    lead = next;
    // The new first frame must be full.
    if (plan[lead].kind == PlanKind::kRepeat) {
      plan[lead].kind =
          plan[lead].pitch != 0 ? PlanKind::kVoiced : PlanKind::kUnvoiced;
    }
  }
  return {.lead = static_cast<int>(lead), .dropped = dropped};
}

Encoder::Encoder(IFrameAnalyser& analyser, IPitchTracker& pitch_tracker,
                 IVoicingClassifier& voicing_classifier,
                 IRepeatSearch& repeat_search, ILevelSetter& level_setter)
    : analyser_(&analyser),
      pitch_tracker_(&pitch_tracker),
      voicing_classifier_(&voicing_classifier),
      repeat_search_(&repeat_search),
      level_setter_(&level_setter) {}

StatusOr<EncodeResult> Encoder::Encode(std::span<const double> samples,
                                       int sample_rate,
                                       const EncoderOptions& options) {
  const auto log = Logger(kLogModuleEncoder);
  if (sample_rate != kEncoderSampleRate) {
    return InvalidArgumentError("the encoder takes 8000 Hz audio, not " +
                                std::to_string(sample_rate) + " Hz");
  }
  if (!std::ranges::all_of(samples,
                           [](double v) { return std::isfinite(v); })) {
    return InvalidArgumentError("the audio holds a sample that is not finite");
  }
  if (Status status = ValidateEncoderOptions(options); !status.IsOk()) {
    return status;
  }
  log->debug(
      "encoding {} samples: targetRms {}, targetPeak {}, peakLimit {}, "
      "repeats {}, repeatLambda {}, gateDb {}, squelchDb {}, preEmphasis {}, "
      "window {}, lagWindowHz {}, voicingThreshold {}, voicingEvidence {}, "
      "iterations {}, fadeUnvoiced {}, quietPauses {}, levelAgc {}, "
      "compression {}",
      samples.size(), options.target_rms,
      options.target_peak.has_value() ? std::to_string(*options.target_peak)
                                      : std::string("none"),
      options.peak_limit, options.repeats, options.repeat_lambda,
      options.gate_db,
      options.squelch_db.has_value() ? std::to_string(*options.squelch_db)
                                     : std::string("none"),
      options.pre_emphasis.chirp ? std::string("chirp")
                                 : std::to_string(options.pre_emphasis.mu),
      options.window, options.lag_window_hz, options.voicing_threshold,
      options.voicing_evidence, options.iterations, options.fade_unvoiced,
      options.quiet_pauses, options.level_agc, options.compression);

  // The squelched source, or the samples as given when there is no
  // squelch (without a copy).
  std::vector<double> squelched;
  std::span<const double> source = samples;
  if (options.squelch_db.has_value()) {
    squelched = SquelchedSource(samples, options);
    source = squelched;
  }

  EncodeResult result;
  std::optional<Segmentation> seg = analyser_->Segment(source, options);
  if (!seg.has_value()) {
    result.frames = {lpc::Frame::Stop()};
    log->debug("encoder: no sound; a lone stop frame");
    return result;
  }
  const PitchTrack track = pitch_tracker_->Track(seg->x, options);
  const std::vector<FrameVoicing> voicing =
      voicing_classifier_->Classify(track, seg->centres);
  const std::vector<FrameFeatures> feats =
      analyser_->Features(*seg, voicing, options);
  std::vector<PlanEntry> plan = repeat_search_->Plan(feats, options);
  if (plan.empty()) {
    result.frames = {lpc::Frame::Stop()};
    log->debug("encoder: every frame is silent; a lone stop frame");
    return result;
  }

  std::vector<double> all_levels;
  all_levels.reserve(plan.size());
  for (std::size_t j = 0; j < plan.size(); ++j) {
    all_levels.push_back(feats[j].level);
  }
  const OpeningTrim trim = TrimFaintOpening(plan, all_levels);
  const auto lead = static_cast<std::size_t>(trim.lead);
  plan.erase(plan.begin(), plan.begin() + trim.lead);
  const std::vector<double> raw_levels(
      all_levels.begin() + trim.lead,
      all_levels.begin() + trim.lead +
          static_cast<std::ptrdiff_t>(plan.size()));
  std::vector<double> levels =
      level_setter_->ShapeLevels(raw_levels, plan, options);

  std::vector<double> squares;
  for (std::size_t j = 0; j < plan.size(); ++j) {
    if (plan[j].kind != PlanKind::kSilence) {
      squares.push_back(levels[j] * levels[j]);
    }
  }
  const double in_rms = squares.empty() ? 1.0 : std::sqrt(dsp::Mean(squares));
  EncoderOptions opts = options;
  double gain = 0.0;
  if (options.target_peak.has_value()) {
    // Scale by RMS first, then rescale so the loudest peak meets the
    // target.
    EncoderOptions first_pass = options;
    first_pass.peak_limit = kPeakPassLimit;
    gain = kPeakPassRms / in_rms;
    const std::vector<int> e =
        level_setter_->SetLevels(plan, levels, gain, first_pass, nullptr);
    const synth::ReferenceSynthesis out =
        synth::ReferenceSynthesizer::Synthesize(
            AddFades(ToFrames(plan, e), first_pass.fade_unvoiced));
    int peak = 1;
    for (const int c : out.codes) {
      peak = std::max(peak, std::abs(c));
    }
    gain *= *options.target_peak / static_cast<double>(peak);
    opts.peak_limit = std::min(options.peak_limit, *options.target_peak);
    log->debug("encoder: first-pass peak {}, gain {:.6g}", peak, gain);
  } else {
    gain = options.target_rms / in_rms;
  }

  LevelStats stats;
  std::vector<int> energies =
      level_setter_->SetLevels(plan, levels, gain, opts, &stats);
  result.frames = AddFades(ToFrames(plan, energies), opts.fade_unvoiced);
  if (opts.quiet_pauses) {
    result.frames = QuietPauses(result.frames);
  }

  EncodeDetails details;
  details.x = std::move(seg->x);
  details.centres.assign(seg->centres.begin() + trim.lead,
                         seg->centres.begin() + trim.lead +
                             static_cast<std::ptrdiff_t>(plan.size()));
  details.c0 = seg->centres[lead];
  details.lead = trim.lead;
  details.dropped_opening = trim.dropped;
  details.gain = gain;
  details.plan = std::move(plan);
  details.levels = std::move(levels);
  details.energies = std::move(energies);
  details.stats = stats;
  result.details = std::move(details);
  log->debug("encoder: {} frames, {} bits, gain {:.6g}", result.frames.size(),
             StreamBits(result.frames), gain);
  return result;
}

std::vector<double> SquelchedSource(std::span<const double> samples,
                                    const EncoderOptions& options) {
  if (!options.squelch_db.has_value()) {
    return {samples.begin(), samples.end()};
  }
  return dsp::Squelch(samples, kEncoderSampleRate, *options.squelch_db);
}

std::vector<dsp::SampleSpan> SquelchedSpans(std::span<const double> samples,
                                            const EncoderOptions& options) {
  if (!options.squelch_db.has_value()) {
    return {};
  }
  return dsp::SquelchedSpans(samples, kEncoderSampleRate, *options.squelch_db);
}

std::unique_ptr<IEncoder> MakeEncoder() {
  return std::make_unique<StandardEncoder>();
}

}  // namespace speak5220::core::encoder
