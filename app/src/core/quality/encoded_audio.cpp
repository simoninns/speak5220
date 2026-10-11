/*
 * File:        encoded_audio.cpp
 * Module:      quality
 * Purpose:     Speaks a phrase's frames through the emulator and lines the
 *              output up with the phrase's source audio, for display and
 *              playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/encoded_audio.h"

#include <algorithm>
#include <cstdint>
#include <ranges>
#include <span>
#include <utility>
#include <vector>

#include "core/encoder/encoder_filters.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/quality/output_alignment.h"
#include "core/synth/synth.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

namespace {

// The chip's idle output code.
constexpr int kIdleCode = -1;
// Full scale of a 16-bit sample.
constexpr double kFullScale = 32768.0;

}  // namespace

StatusOr<EncodedAudio> RenderEncodedAudio(synth::ISynth& synth,
                                          std::span<const float> source,
                                          std::span<const lpc::Frame> frames) {
  StatusOr<std::vector<std::uint8_t>> packed = lpc::FrameCodec::Pack(frames);
  if (!packed.IsOk()) {
    return packed.GetStatus();
  }
  StatusOr<synth::SynthResult> spoken =
      synth.Synthesize(*packed, synth::SynthPath::kVsm);
  if (!spoken.IsOk()) {
    return spoken.GetStatus();
  }
  EncodedAudio audio{.samples = std::move(spoken->samples),
                     .source_offset = kFirstFrameStretch};

  const std::vector<int>& codes = spoken->codes;
  const auto first =
      std::ranges::find_if(codes, [](int c) { return c != kIdleCode; });
  if (source.empty() || first == codes.end()) {
    return audio;
  }
  // The sound, idle at 0, lined up with the source after the encoder's
  // 60 Hz high-pass, as PesqMetric does (speechqual.py measure()).
  const auto last = std::ranges::find_if(std::views::reverse(codes),
                                         [](int c) { return c != kIdleCode; });
  std::vector<double> output;
  output.reserve(static_cast<std::size_t>(last.base() - first));
  for (auto it = first; it != last.base(); ++it) {
    output.push_back(static_cast<double>(*it) + 1.0);
  }
  const std::vector<double> input(source.begin(), source.end());
  const std::vector<double> reference = encoder::HighPass60Hz(input);
  const int lag =
      RefineBySpectrum(reference, output, AlignByEnvelope(reference, output));
  // Source sample i lines up with output[i - lag], which is
  // codes[(first - codes.begin()) + i - lag].
  audio.source_offset = static_cast<int>(first - codes.begin()) - lag;
  // The gain is measured in codes; a sample is codes x kSampleScale of
  // 16-bit full scale.
  const double code_gain = LevelMatchGain(
      reference, Place(output, lag, static_cast<int>(reference.size())));
  audio.level_gain = code_gain * kFullScale / synth::kSampleScale;
  Logger(kLogModuleQuality)
      ->debug(
          "encoded audio: {} samples, source sample 0 at {}, level "
          "gain {:.3f}",
          audio.samples.size(), audio.source_offset, audio.level_gain);
  return audio;
}

}  // namespace speak5220::core::quality
