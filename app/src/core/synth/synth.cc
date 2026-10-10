/*
 * File:        synth.cc
 * Module:      synth
 * Purpose:     Renders a phrase's packed bytes to 8 kHz samples through the
 *              emulated TMS5220, by the VSM or the Speak External path
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/synth/synth.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/emulator/chip_driver.h"
#include "core/emulator/sample_sink.h"
#include "core/emulator/tms5220.h"
#include "core/emulator/tms5220_event.h"
#include "core/emulator/tms6100.h"
#include "core/util/logging.h"

namespace speak5220::core::synth {
namespace {

// Codes are scaled to 16-bit samples by 64, as the encoder's --wav does.
constexpr int kSampleScale = 64;
// The idle output code (§8.8: u1 = -9).
constexpr int kIdleCode = -1;
// FNV-1a, 64 bits.
constexpr std::uint64_t kFnvOffset = 14695981039346656037ULL;
constexpr std::uint64_t kFnvPrime = 1099511628211ULL;

// Records the codes from the sample period in which the first Speak (or
// Speak External) takes effect: the period of the first frame read.
class Recorder : public emulator::ISampleSink,
                 public emulator::ITms5220Observer {
 public:
  void OnEvent(const emulator::Tms5220Event& event) override {
    if (event.type != emulator::EventType::kSpeech || first_period_) {
      return;
    }
    if (event.milestone == emulator::Milestone::kStartSpeak ||
        event.milestone == emulator::Milestone::kStartExternal) {
      first_period_ = event.slot / emulator::Tms5220::kSlotsPerSample;
    }
  }

  void OnSample(std::int64_t index, std::int64_t /*slot*/, int code) override {
    if (first_period_ && index >= *first_period_) {
      codes_.push_back(code);
    }
  }

  bool Started() const { return first_period_.has_value(); }

  SynthResult TakeResult() {
    SynthResult result;
    result.first_period = first_period_.value_or(0);
    result.codes = std::move(codes_);
    result.samples.reserve(result.codes.size());
    result.leading_idle_samples = result.codes.size();
    for (std::size_t i = 0; i < result.codes.size(); ++i) {
      const int code = result.codes[i];
      result.samples.push_back(static_cast<std::int16_t>(code * kSampleScale));
      if (code != kIdleCode &&
          result.leading_idle_samples == result.codes.size()) {
        result.leading_idle_samples = i;
      }
    }
    return result;
  }

 private:
  std::optional<std::int64_t> first_period_;
  std::vector<int> codes_;
};

std::string_view PathName(SynthPath path) {
  return path == SynthPath::kVsm ? "VSM" : "Speak External";
}

}  // namespace

std::size_t Synth::CacheKeyHash::operator()(const CacheKey& key) const {
  std::uint64_t hash = kFnvOffset;
  hash = (hash ^ static_cast<std::uint64_t>(key.path)) * kFnvPrime;
  for (const std::uint8_t byte : key.bytes) {
    hash = (hash ^ byte) * kFnvPrime;
  }
  return static_cast<std::size_t>(hash);
}

Synth::Synth(const emulator::Tms5220Config& config) : config_(config) {}

StatusOr<SynthResult> Synth::SpeakFromVsm(emulator::IVsmBus& vsm,
                                          std::uint32_t address) const {
  Recorder recorder;
  emulator::ChipDriver driver(config_, &vsm, &recorder, &recorder);
  if (Status status = driver.SpeakFromVsm(address, {}); !status.IsOk()) {
    return status;
  }
  if (!recorder.Started()) {
    return InternalError("the Speak command did not start");
  }
  return recorder.TakeResult();
}

StatusOr<SynthResult> Synth::SpeakExternal(std::span<const std::uint8_t> stream,
                                           emulator::IVsmBus& vsm) const {
  Recorder recorder;
  emulator::ChipDriver driver(config_, &vsm, &recorder, &recorder);
  if (Status status = driver.SpeakExternal(stream, {}); !status.IsOk()) {
    return status;
  }
  if (!recorder.Started()) {
    // speak5220 datasheet §12.9: a Speak External stream needs at least 9
    // bytes before speech starts.
    return FailedPreconditionError(
        "the Speak External stream never started: it needs at least 9 bytes "
        "(datasheet §12.9), and has " +
        std::to_string(stream.size()));
  }
  return recorder.TakeResult();
}

StatusOr<SynthResult> Synth::Synthesize(std::span<const std::uint8_t> phrase,
                                        SynthPath path) {
  if (phrase.empty()) {
    return InvalidArgumentError("cannot synthesise an empty phrase");
  }
  if (phrase.size() > emulator::Tms6100::kMemorySize) {
    return OutOfRangeError("phrase of " + std::to_string(phrase.size()) +
                           " bytes does not fit the VSM's 1 MB");
  }
  CacheKey key{
      .path = path,
      .bytes = std::vector<std::uint8_t>(phrase.begin(), phrase.end())};
  {
    const std::scoped_lock lock(mutex_);
    if (auto found = cache_.find(key); found != cache_.end()) {
      return *found->second;
    }
  }
  emulator::Tms6100 vsm;
  StatusOr<SynthResult> result = InternalError("unknown synthesis path");
  if (path == SynthPath::kVsm) {
    vsm.LoadMemory(phrase, 0);
    result = SpeakFromVsm(vsm, 0);
  } else {
    result = SpeakExternal(phrase, vsm);
  }
  if (!result.IsOk()) {
    return result;
  }
  Logger(kLogModuleSynth)
      ->debug("synthesised {} bytes by the {} path: {} samples, {} idle first",
              phrase.size(), PathName(path), result->codes.size(),
              result->leading_idle_samples);
  {
    const std::scoped_lock lock(mutex_);
    cache_.emplace(std::move(key),
                   std::make_shared<const SynthResult>(*result));
  }
  return result;
}

std::size_t Synth::CacheSize() const {
  const std::scoped_lock lock(mutex_);
  return cache_.size();
}

}  // namespace speak5220::core::synth
