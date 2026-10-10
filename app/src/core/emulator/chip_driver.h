/*
 * File:        chip_driver.h
 * Module:      emulator
 * Purpose:     Drives a TMS5220 as a host does: whole host cycles, waiting
 *              for idle and speaking a phrase
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_CHIP_DRIVER_H_
#define SPEAK5220_CORE_EMULATOR_CHIP_DRIVER_H_

#include <cstdint>
#include <functional>
#include <span>
#include <string_view>

#include "core/emulator/sample_sink.h"
#include "core/emulator/tms5220.h"
#include "core/emulator/tms5220_config.h"
#include "core/emulator/tms5220_event.h"
#include "core/emulator/vsm_bus.h"
#include "core/util/status.h"

namespace speak5220::core::emulator {

// How a phrase is spoken: the procedure of tms5220emu's bulk mode
// (docs/technical/emulator/command-line.md).
struct SpeakOptions {
  // The command is latched this many slots before a VSM frame-read slot:
  // 1961, the standard procedure of speak5220 datasheet §7.
  std::int64_t speak_at = 1961;
  // Slots run after the chip is idle again: one frame.
  std::int64_t margin = Tms5220::kSlotsPerFrame;
  // Write the reset sequence (nine 0xFF, then 0x70) first.
  bool reset = false;
};

// A host for one Tms5220: it runs host cycles to completion and steps the
// chip between them. Re-implements the operations of
// investigation/emulator/tms5220emu.c. The VSM bus, sink and observer are
// borrowed, may be null, and must outlive the driver. Not thread-safe.
class ChipDriver : private ISampleSink {
 public:
  // A host gives up waiting after 60 s of chip time.
  static constexpr std::int64_t kMaxWaitSlots = 160000LL * 60;
  // Idle means this many samples of code -1 in a row (and no speech).
  static constexpr int kIdleSamples = 25;
  // §12.9 Speak External: the FIFO is filled to 16 bytes, then topped up
  // by 8 whenever a status read every 1 ms (160 slots) shows Buffer Low.
  static constexpr int kFifoFill = 16;
  static constexpr int kFifoTopUp = 8;
  static constexpr std::int64_t kStatusPollSlots = 160;

  ChipDriver(const Tms5220Config& config, IVsmBus* vsm, ISampleSink* sink,
             ITms5220Observer* observer);

  ChipDriver(const ChipDriver&) = delete;
  ChipDriver& operator=(const ChipDriver&) = delete;
  ~ChipDriver() override = default;

  Tms5220& Chip() { return chip_; }

  // Receives the name of each mark (`mark`, and the "speak" or
  // "speak_external" mark of the speak procedures) and the current slot.
  void SetMarkHandler(
      std::function<void(std::int64_t slot, std::string_view name)> handler);
  void Mark(std::string_view name);

  // Steps until the chip's next slot is `slot`.
  void StepTo(std::int64_t slot);
  // A host write or read at the current slot, run until READY goes active.
  // The next operation starts at the following slot. A cycle that has not
  // finished after kMaxWaitSlots is abandoned with a warning.
  // Re-implement op_write() and op_read().
  Status Write(int byte);
  StatusOr<int> Read();
  // Runs until Talk Status is clear, no speech is running or pending and
  // the last 25 codes were -1, then `margin` more slots. Re-implements
  // op_until_idle().
  void UntilIdle(std::int64_t margin);
  // Advances so that a command written now is latched exactly `lead` slots
  // before the next VSM frame-read slot far enough ahead. Re-implements
  // op_boundary().
  void Boundary(std::int64_t lead);

  // Speaks the phrase at `address` with Speak: waits for idle, writes Load
  // Address (five nibbles, low first), then Speak at the boundary, and runs
  // until idle. Re-implements bulk() without --external.
  Status SpeakFromVsm(std::uint32_t address, const SpeakOptions& options);
  // Speaks `stream` with Speak External: the command at the boundary, then
  // the bytes as SpeakOptions describes. Re-implements bulk() with
  // --external.
  Status SpeakExternal(std::span<const std::uint8_t> stream,
                       const SpeakOptions& options);

 private:
  void OnSample(std::int64_t index, std::int64_t slot, int code) override;
  void FinishCycle();
  Status ResetSequence();

  ISampleSink* sink_;
  std::function<void(std::int64_t, std::string_view)> mark_handler_;
  int idle_run_ = 0;
  Tms5220 chip_;
};

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_CHIP_DRIVER_H_
