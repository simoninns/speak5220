/*
 * File:        synth.h
 * Module:      synth
 * Purpose:     Renders a phrase's packed bytes to 8 kHz samples through the
 *              emulated TMS5220, by the VSM or the Speak External path
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_SYNTH_SYNTH_H_
#define SPEAK5220_CORE_SYNTH_SYNTH_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>

#include "core/emulator/tms5220_config.h"
#include "core/emulator/vsm_bus.h"
#include "core/util/status.h"

namespace speak5220::core::synth {

// Codes are scaled to 16-bit samples by this, as the encoder's --wav does:
// the DAC's +-127 codes take a quarter of 16-bit full scale.
inline constexpr int kSampleScale = 64;

// How the phrase reaches the chip (plan §2.3).
enum class SynthPath {
  // Loaded into a TMS6100 at address 0 and spoken with Load Address x 5,
  // Speak: how a PHROM word is spoken. The default for playback.
  kVsm,
  // Written to the FIFO with Speak External (speak5220 datasheet §12.9),
  // as the BBC BASIC export does. Starts 2 samples earlier, with different
  // noise.
  kSpeakExternal,
};

// What the chip produced for one phrase.
struct SynthResult {
  // I/O codes, one per 8 kHz sample, from the sample period in which the
  // first frame is read until the output has been idle (code -1) for 25
  // samples and a frame more has run.
  std::vector<int> codes;
  // The same as 16-bit samples: codes x 64, the convention of the
  // encoder's --wav output.
  std::vector<std::int16_t> samples;
  // The leading idle: the first sample that differs from code -1, about 25
  // ms in. The waveform view uses it to align the encoded audio with the
  // original. Equal to codes.size() when the phrase never leaves idle.
  std::size_t leading_idle_samples = 0;
  // The sample period (since power-up) of codes[0].
  std::int64_t first_period = 0;
};

// Renders packed phrases. Implementations are thread-safe.
class ISynth {
 public:
  virtual ~ISynth() = default;

  // Renders `phrase` (packed frame bytes, as FrameCodec::Pack() gives) by
  // `path`.
  virtual StatusOr<SynthResult> Synthesize(std::span<const std::uint8_t> phrase,
                                           SynthPath path) = 0;
};

// Whether Synth keeps its results.
enum class SynthCache {
  // Kept per phrase: playback speaks the same phrases again and again.
  kOn,
  // Not kept: the optimiser speaks hundreds of candidates once each,
  // which would otherwise stay in memory.
  kOff,
};

// Runs Tms5220 and Tms6100 on a phrase, from power-up, with the procedure
// of the reference emulator's bulk mode (emulator::ChipDriver), so the
// codes match the reference emulator sample for sample. Results are cached
// per phrase (unless made with SynthCache::kOff), keyed by a hash of the
// packed bytes and the path; a cache hit is checked against the full
// bytes. Thread-safe: renders run in parallel, the cache is guarded by a
// mutex. Not on the GUI thread: a phrase takes milliseconds per second of
// speech.
class Synth : public ISynth {
 public:
  explicit Synth(const emulator::Tms5220Config& config = {},
                 SynthCache cache = SynthCache::kOn);

  StatusOr<SynthResult> Synthesize(std::span<const std::uint8_t> phrase,
                                   SynthPath path) override;

  // The VSM path against a given VSM bus, holding the phrase at
  // `address`. Uncached.
  StatusOr<SynthResult> SpeakFromVsm(emulator::IVsmBus& vsm,
                                     std::uint32_t address) const;
  // The Speak External path, writing `stream` to the FIFO. `vsm` is
  // connected to the chip but a Speak External uses no VSM function.
  // Uncached.
  StatusOr<SynthResult> SpeakExternal(std::span<const std::uint8_t> stream,
                                      emulator::IVsmBus& vsm) const;

  // The number of cached results.
  std::size_t CacheSize() const;

 private:
  struct CacheKey {
    SynthPath path = SynthPath::kVsm;
    std::vector<std::uint8_t> bytes;
    friend bool operator==(const CacheKey& a, const CacheKey& b) = default;
  };
  struct CacheKeyHash {
    std::size_t operator()(const CacheKey& key) const;
  };

  emulator::Tms5220Config config_;
  SynthCache cache_policy_;
  mutable std::mutex mutex_;
  std::unordered_map<CacheKey, std::shared_ptr<const SynthResult>, CacheKeyHash>
      cache_;
};

}  // namespace speak5220::core::synth

#endif  // SPEAK5220_CORE_SYNTH_SYNTH_H_
