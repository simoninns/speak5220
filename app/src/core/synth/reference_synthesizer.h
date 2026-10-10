/*
 * File:        reference_synthesizer.h
 * Module:      synth
 * Purpose:     The encoder's own synthesiser: the datasheet's reference
 *              algorithm run directly on a frame list
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_SYNTH_REFERENCE_SYNTHESIZER_H_
#define SPEAK5220_CORE_SYNTH_REFERENCE_SYNTHESIZER_H_

#include <array>
#include <optional>
#include <span>
#include <vector>

#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"

namespace speak5220::core::synth {

// The output of one reference synthesis.
struct ReferenceSynthesis {
  // I/O codes from the sample period in which the first frame is read
  // (sample 0). The output stays idle (-1) until about sample 206; frame
  // j's values are reached around sample 213 + 200 j. The codes end once
  // the output has been back at idle for 25 samples after the stop.
  std::vector<int> codes;
  // The number of samples in which a lattice sum wrapped (§8.8).
  int wrapped_samples = 0;
  // The largest magnitude of a lattice sum before its wrap.
  int peak_internal = 0;
};

// Runs speak5220 datasheet §8.5-§8.9 for one VSM Speak from power-up,
// without the slot-level emulator: much faster, for the encoder's closed
// loop. Its codes match the emulator's VSM path from the first frame read
// (Synth with SynthPath::kVsm). Stateless; thread-safe.
class ReferenceSynthesizer {
 public:
  // Synthesises `frames`, which need not end in a stop: running out of
  // frames ends the speech as a stop does. `initial_indices` sets the
  // parameter RAM (energy, pitch, K1-K10) left by a previous utterance;
  // all zero by default. Re-implements tms5220enc.py _synth() and
  // synthesize(). O(frames).
  static ReferenceSynthesis Synthesize(
      std::span<const lpc::Frame> frames,
      const std::optional<std::array<int, lpc::kParameterCount>>&
          initial_indices = std::nullopt);
};

}  // namespace speak5220::core::synth

#endif  // SPEAK5220_CORE_SYNTH_REFERENCE_SYNTHESIZER_H_
