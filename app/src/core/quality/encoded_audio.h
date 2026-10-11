/*
 * File:        encoded_audio.h
 * Module:      quality
 * Purpose:     Speaks a phrase's frames through the emulator and lines the
 *              output up with the phrase's source audio, for display and
 *              playback
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_ENCODED_AUDIO_H_
#define SPEAK5220_CORE_QUALITY_ENCODED_AUDIO_H_

#include <cstdint>
#include <span>
#include <vector>

#include "core/encoder/encoder_types.h"
#include "core/lpc/frame.h"
#include "core/synth/synth.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// The encoded sample at which frame 0's own stretch begins: its values are
// half-way reached there (encoder::kReachedSample).
inline constexpr int kFirstFrameStretch =
    encoder::kReachedSample - (encoder::kFrameSamples / 2);

// A phrase's encoded sound, placed against its source audio.
struct EncodedAudio {
  // The chip's output through the VSM path (SynthResult::samples): codes x
  // 64 at 8 kHz, from the sample period in which the first frame is read.
  std::vector<std::int16_t> samples;
  // The index in `samples` that lines up with source sample 0, so source
  // sample s sounds at samples[s + source_offset]. It can be negative, or
  // past the end.
  int source_offset = 0;
  // The gain that brings `samples` (as fractions of 16-bit full scale) to
  // the source's level: the mean power over the source's speech, as PESQ
  // matches levels (LevelMatchGain()). 1 with no source or no sound.
  double level_gain = 1.0;
};

// Speaks `frames` (ending in a stop) through `synth`'s VSM path and finds
// where the output lines up with `source` (8 kHz). The offset is the
// chip's start latency plus wherever the encoder put its first frame (it
// leaves out leading silence, and the project does not record where), so
// it is found by the alignment PESQ uses (AlignOutput()'s lag) rather than
// assumed. The level gain is measured on the output so placed. With no
// source, or an output that never leaves idle, the offset is
// kFirstFrameStretch (frame 0's stretch starts at source sample 0) and the
// gain 1.
// Fails if the frames cannot be packed or spoken. About 0.04 s per second
// of speech; not on the GUI thread. Thread-safe if the synth is.
StatusOr<EncodedAudio> RenderEncodedAudio(synth::ISynth& synth,
                                          std::span<const float> source,
                                          std::span<const lpc::Frame> frames);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_ENCODED_AUDIO_H_
