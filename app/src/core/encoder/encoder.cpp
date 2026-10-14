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
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/dsp/array_math.h"
#include "core/dsp/fft.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_endings.h"
#include "core/encoder/voicing_classifier.h"
#include "core/lpc/frame.h"
#include "core/preprocess/preprocess.h"
#include "core/quality/pitch_estimator.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::core::encoder {
namespace {

// A faint opening is more than this far below the loudest frame.
constexpr double kFaintOpeningDb = -36.0;

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

// The stages MakeEncoder() owns. The pitch tracker's anchor is RAPT: at
// 8 kHz REAPER, which the quality measures use, loses the voicing of whole
// words and drops an octave on low and high voices, and Harvest, as right
// as RAPT on the octave, takes five times as long as the rest of the
// encoder (docs-tech/design/high-level-design.md §8, "The pitch anchor").
class StandardEncoder : public IEncoder {
 public:
  StandardEncoder()
      : pitch_tracker_(fft_, rapt_),
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
  quality::SptkPitchEstimator rapt_{quality::PitchAlgorithm::kRapt};
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
      "encoding {} samples: targetRms {}, repeatLambda {}, gateDb {}, "
      "noiseGateDb {}, noiseGateMs {}, bassCutHz {}, trebleTilt {}, "
      "deEssDb {}, preEmphasis {}, window {}, "
      "lagWindowHz {}, resonanceHoldDb {}, voicingThreshold {}, "
      "voicingEvidence {}, voicingMinRun {}, levelAgc {}",
      samples.size(), options.target_rms, options.repeat_lambda,
      options.gate_db,
      options.preprocess.noise_gate_db.has_value()
          ? std::to_string(*options.preprocess.noise_gate_db)
          : std::string("none"),
      options.preprocess.noise_gate_ms,
      options.preprocess.bass_cut_hz.has_value()
          ? std::to_string(*options.preprocess.bass_cut_hz)
          : std::string("none"),
      options.preprocess.treble_tilt,
      options.preprocess.de_ess_db.has_value()
          ? std::to_string(*options.preprocess.de_ess_db)
          : std::string("none"),
      options.pre_emphasis.chirp ? std::string("chirp")
                                 : std::to_string(options.pre_emphasis.mu),
      options.window, options.lag_window_hz,
      options.resonance_hold_db.has_value()
          ? std::to_string(*options.resonance_hold_db)
          : std::string("none"),
      options.voicing_threshold, options.voicing_evidence,
      options.voicing_min_run, options.level_agc);

  const preprocess::PreprocessedAudio source =
      preprocess::Preprocess(samples, options.preprocess);
  const bool filtered = options.preprocess.bass_cut_hz.has_value() ||
                        options.preprocess.treble_tilt > 0.0 ||
                        options.preprocess.de_ess_db.has_value();

  EncodeResult result;
  std::optional<Segmentation> seg = analyser_->Segment(source.voicing, options);
  if (!seg.has_value()) {
    result.frames = {lpc::Frame::Stop()};
    log->debug("encoder: no sound; a lone stop frame");
    return result;
  }
  const PitchTrack track = pitch_tracker_->Track(seg->x, options);
  std::vector<FrameVoicing> voicing =
      voicing_classifier_->Classify(track, seg->centres);
  SmoothVoicingRuns(voicing, options.voicing_min_run);
  // With the recording filtered or de-essed, the spectra, levels and gate
  // come from the preprocessed recording; the speech's extent, pitch and
  // voicing stay as the recording before its filters gives them.
  if (filtered) {
    if (std::optional<Segmentation> filtered_seg =
            analyser_->Segment(source.samples, options);
        filtered_seg.has_value()) {
      seg->x = std::move(filtered_seg->x);
      seg->gate = filtered_seg->gate;
      seg->loud = filtered_seg->loud;
    }
  }
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
  if (options.resonance_hold_db.has_value()) {
    HoldResonantFilters(plan, raw_levels, *options.resonance_hold_db);
  }
  // The energies aim at each frame's loudness (its level after the
  // loudness high-pass), so bass does not count towards target_rms.
  const std::vector<double> loudness = [&] {
    std::vector<double> out;
    out.reserve(plan.size());
    for (std::size_t j = 0; j < plan.size(); ++j) {
      out.push_back(feats[lead + j].loudness);
    }
    return out;
  }();
  std::vector<double> levels =
      level_setter_->ShapeLevels(loudness, plan, options);

  std::vector<double> squares;
  for (std::size_t j = 0; j < plan.size(); ++j) {
    if (plan[j].kind != PlanKind::kSilence) {
      squares.push_back(levels[j] * levels[j]);
    }
  }
  const double in_rms = squares.empty() ? 1.0 : std::sqrt(dsp::Mean(squares));
  const double gain = options.target_rms / in_rms;

  LevelStats stats;
  std::vector<int> energies =
      level_setter_->SetLevels(plan, levels, gain, &stats);
  result.frames = QuietPauses(AddFades(ToFrames(plan, energies)));

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
  log->debug(
      "encoder: {} frames, {} bits, gain {:.6g}; limiter lowered {} and "
      "silenced {} of {} sounding frames",
      result.frames.size(), StreamBits(result.frames), gain, stats.limited,
      stats.silenced, stats.sounding);
  return result;
}

std::unique_ptr<IEncoder> MakeEncoder() {
  return std::make_unique<StandardEncoder>();
}

}  // namespace speak5220::core::encoder
