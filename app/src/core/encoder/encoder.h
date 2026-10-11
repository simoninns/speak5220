/*
 * File:        encoder.h
 * Module:      encoder
 * Purpose:     Encodes 8 kHz speech into TMS5220 frames by composing the
 *              analysis, pitch, voicing, repeat and level stages
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_ENCODER_H_
#define SPEAK5220_CORE_ENCODER_ENCODER_H_

#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "core/dsp/squelch.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/frame_analyser.h"
#include "core/encoder/level_setter.h"
#include "core/encoder/pitch_tracker.h"
#include "core/encoder/repeat_search.h"
#include "core/encoder/voicing_classifier.h"
#include "core/lpc/frame.h"
#include "core/util/status.h"

namespace speak5220::core::encoder {

// The version of the encoder's output. A project's cached encodings record
// it, and are redone when it changes (plan §3.4), so it changes whenever
// the same audio and options can give different frames (AGENTS.md §3.1).
// 1.1.0: quiet pauses (QuietPauses()), on by default.
inline constexpr std::string_view kEncoderVersion = "1.1.0";

// The analysis behind an encoding, for tests and the waveform view.
// Output sample kReachedSample + 200 j of the reference synthesis
// corresponds to input sample centres[j].
struct EncodeDetails {
  // The input after the 60 Hz high-pass.
  std::vector<double> x;
  // Each planned frame's centre, in input samples.
  std::vector<int> centres;
  // The first frame's centre, and how many analysed frames came before it
  // (silence, or a faint opening that was dropped).
  int c0 = 0;
  int lead = 0;
  int dropped_opening = 0;
  // The level, in codes per unit of input RMS.
  double gain = 0.0;
  // One entry per planned frame: kind, K and pitch; the levels after
  // shaping; and the energy indices.
  std::vector<PlanEntry> plan;
  std::vector<double> levels;
  std::vector<int> energies;
  LevelStats stats;
};

// An encoding: frames ending in a stop, and the analysis behind them
// (absent when the input held no sound).
struct EncodeResult {
  std::vector<lpc::Frame> frames;
  std::optional<EncodeDetails> details;
};

// Encodes speech. Implementations are deterministic: the same input and
// options always give the same frames.
class IEncoder {
 public:
  virtual ~IEncoder() = default;

  // Encodes `samples` (-1..1) at `sample_rate`, which must be 8000 (the
  // import pipeline resamples, plan §3.1). Fails for another rate, for
  // non-finite samples or for options outside their ranges. Input with no
  // sound gives a lone stop frame.
  virtual StatusOr<EncodeResult> Encode(std::span<const double> samples,
                                        int sample_rate,
                                        const EncoderOptions& options) = 0;
};

// Where the plan starts once a faint opening is dropped.
struct OpeningTrim {
  // The first planned frame kept.
  int lead = 0;
  // Sounding and silent frames dropped after the leading silence.
  int dropped = 0;
};

// Drops a faint opening sound (all frames more than 36 dB below the
// loudest) with no voiced frame that is followed by silence: kept, it
// would need energy-1 hiss through the pause (speak5220 datasheet §12.2).
// Skips leading silence too. If the new first frame is a repeat, it is
// re-planned as a full frame. `plan` must hold a sounding frame.
// Re-implements part of tms5220enc.py encodeDetailed().
OpeningTrim TrimFaintOpening(std::vector<PlanEntry>& plan,
                             std::span<const double> levels);

// IEncoder over injected stages, in the order of tms5220enc.py
// encodeDetailed(): segment, track pitch, classify voicing, measure
// features, plan, drop a faint opening, shape the levels, set the gain
// (by RMS, or by a first pass for a target peak), set the energies, then
// fades and the stop. Thread-safe if the stages are.
class Encoder : public IEncoder {
 public:
  // The stages must outlive the encoder.
  Encoder(IFrameAnalyser& analyser, IPitchTracker& pitch_tracker,
          IVoicingClassifier& voicing_classifier, IRepeatSearch& repeat_search,
          ILevelSetter& level_setter);

  StatusOr<EncodeResult> Encode(std::span<const double> samples,
                                int sample_rate,
                                const EncoderOptions& options) override;

 private:
  IFrameAnalyser* analyser_;
  IPitchTracker* pitch_tracker_;
  IVoicingClassifier* voicing_classifier_;
  IRepeatSearch* repeat_search_;
  ILevelSetter* level_setter_;
};

// The source an encoding is made from and scored against: `samples` (8
// kHz) squelched when options.squelch_db is set (dsp::Squelch()), else a
// copy. The encoder applies it itself; whatever measures an encoding
// against its source (PESQ, the anchor) applies it to the source first,
// so breaths muted for the encoder are not asked of it. Pure; O(n).
std::vector<double> SquelchedSource(std::span<const double> samples,
                                    const EncoderOptions& options);

// The stretches of `samples` (8 kHz) that SquelchedSource() lowers, for
// showing what the squelch mutes; none without a squelch. Pure; O(n).
std::vector<dsp::SampleSpan> SquelchedSpans(std::span<const double> samples,
                                            const EncoderOptions& options);

// An IEncoder with the standard stages over pocketfft: the encoder the
// application uses. Stateless between calls; thread-safe.
std::unique_ptr<IEncoder> MakeEncoder();

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_ENCODER_H_
