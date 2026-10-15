/*
 * File:        signal_preparation.h
 * Module:      quality
 * Purpose:     Prepares an original and the chip's output alike for the
 *              quality measures: placement, band-limit, voice activity
 *              mask and level
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_SIGNAL_PREPARATION_H_
#define SPEAK5220_CORE_QUALITY_SIGNAL_PREPARATION_H_

#include <cstdint>
#include <span>
#include <vector>

#include "core/quality/encoded_audio.h"
#include "core/quality/voice_activity.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// The voice activity detector's aggressiveness for the mask
// (FvadVoiceActivity): the most aggressive, which on the corpus marks 1 %
// of the quiet frames more than 100 ms from speech, against 29 % at the
// next.
inline constexpr int kSpeechDetectorMode = 3;

// Frames of the voice activity mask more than this many dB below the
// original's loudest are left out of it: the WebRTC detector keeps the
// decays at the ends of words, where the measures are unreliable. ESTOI
// drops frames by the same margin.
inline constexpr double kMaskFloorDb = 40.0;

// An original and the chip's output prepared alike.
struct PreparedSignals {
  // The source, band-limited (BandLimit()).
  std::vector<double> original;
  // The chip's output placed against the source (PlaceOutput()),
  // band-limited, and scaled to the original's power over the mask. The
  // same length as `original`.
  std::vector<double> output;
  // One flag per whole kVoiceActivityFrame of `original`: true where the
  // measures count it (SpeechMask()).
  std::vector<bool> speech;
};

// The chip's output (SynthResult::samples, codes x 64) placed so that
// sample s of the n returned is samples[s + source_offset], with the idle
// code at 0, as fractions of 16-bit full scale; 0 outside the output.
// Taking the idle code as 0 removes the chip's DC offset.
std::vector<double> PlaceOutput(std::span<const std::int16_t> samples,
                                int source_offset, int n);

// `x` through the shared band-limit, dsp::kQualityBand (order-4
// Butterworth, 80 to 3600 Hz), forwards and backwards, so without delay.
std::vector<double> BandLimit(std::span<const double> x);

// The voice activity mask of `original`: `vad`'s decision for each whole
// kVoiceActivityFrame, less the frames whose mean power is kMaskFloorDb or
// more below the loudest frame's. Fails if the detector does.
StatusOr<std::vector<bool>> SpeechMask(std::span<const double> original,
                                       IVoiceActivity& vad);

// The gain that gives `output` the power of `original` over the samples
// of the frames `speech` keeps; 1 if `output` is silent there.
double MaskedLevelGain(std::span<const double> original,
                       std::span<const double> output,
                       const std::vector<bool>& speech);

// Prepares `source` (8 kHz, -1..1) and `output`, already lined up with it
// and the same length (such as the anchor vocoder's), for the measures:
// both band-limited, the mask, and the output scaled. Fails if the
// detector does.
StatusOr<PreparedSignals> PrepareAlignedSignals(std::span<const double> source,
                                                std::span<const double> output,
                                                IVoiceActivity& vad);

// Prepares `source` (8 kHz, -1..1, as given to the encoder) and `audio`
// (its encoding spoken and placed, RenderEncodedAudio()) for the measures.
// Fails if the detector does. O(n). Thread-safe if `vad` is.
StatusOr<PreparedSignals> PrepareSignals(std::span<const double> source,
                                         const EncodedAudio& audio,
                                         IVoiceActivity& vad);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_SIGNAL_PREPARATION_H_
