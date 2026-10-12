/*
 * File:        phrase_encoder.cpp
 * Module:      encoder_panel
 * Purpose:     Encodes a phrase and measures the result for the GUI: the
 *              cached encoding with its quality, the peak code and
 *              the limiter's activity
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/encoder_panel/phrase_encoder.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/preprocess/preprocess.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/quality/cached_quality.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/synth.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::app {

namespace project = core::project;

EncodingFacts MeasureFrames(const EncodingTools& tools,
                            core::synth::ISynth& synth,
                            std::span<const float> audio,
                            const core::encoder::EncoderOptions& options,
                            const std::vector<core::lpc::Frame>& frames) {
  EncodingFacts facts;
  const std::vector<double> source =
      core::preprocess::Preprocess(
          std::vector<double>(audio.begin(), audio.end()), options.preprocess)
          .samples;
  if (core::StatusOr<core::quality::QualityAssessment> quality =
          tools.quality->Assess(source, frames);
      quality.IsOk()) {
    facts.quality = *quality;
  } else {
    core::Logger(core::kLogModuleGui)
        ->warn("cannot measure the quality: {}", quality.GetStatus().Message());
  }
  if (core::StatusOr<std::vector<std::uint8_t>> packed =
          core::lpc::FrameCodec::Pack(frames);
      packed.IsOk()) {
    if (core::StatusOr<core::synth::SynthResult> spoken =
            synth.Synthesize(*packed, core::synth::SynthPath::kVsm);
        spoken.IsOk() && !spoken->codes.empty()) {
      int peak = 0;
      for (const int code : spoken->codes) {
        peak = std::max(peak, std::abs(code));
      }
      facts.peak_code = peak;
    }
  }
  return facts;
}

core::StatusOr<MeasuredEncoding> EncodePhrase(
    const EncodingTools& tools, core::synth::ISynth& synth,
    std::span<const float> audio,
    const core::encoder::EncoderOptions& options) {
  const std::vector<double> source(audio.begin(), audio.end());
  core::StatusOr<core::encoder::EncodeResult> result =
      tools.encoder->Encode(source, project::kProjectSampleRate, options);
  if (!result.IsOk()) {
    return result.GetStatus();
  }
  MeasuredEncoding measured;
  measured.facts = MeasureFrames(tools, synth, audio, options, result->frames);
  if (const std::optional<core::encoder::EncodeDetails>& details =
          result->details;
      details.has_value()) {
    measured.facts.levels = details->stats;
  }
  measured.encoding = project::EncodingCache::MakeEncoding(
      audio, options, std::move(result->frames));
  if (measured.facts.quality.has_value()) {
    measured.encoding.quality =
        core::quality::ToCachedQuality(*measured.facts.quality);
  }
  return measured;
}

}  // namespace speak5220::app
