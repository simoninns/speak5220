/*
 * File:        chip_driver.cc
 * Module:      emulator
 * Purpose:     Drives a TMS5220 as a host does: whole host cycles, waiting
 *              for idle and speaking a phrase
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/chip_driver.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <span>
#include <string_view>
#include <utility>

#include "core/util/logging.h"

namespace speak5220::core::emulator {
namespace {

// Commands written by the speak procedures (§6).
constexpr int kLoadAddress = 0x40;
constexpr int kSpeak = 0x50;
constexpr int kSpeakExternal = 0x60;
constexpr int kReset = 0x70;
// §6: nine 0xFF bytes end any Speak External in progress before a Reset.
constexpr int kResetPadding = 0xFF;
constexpr int kResetPaddingCount = 9;
// §5: status bit 6, Buffer Low.
constexpr int kBufferLowBit = 0x40;
constexpr int kAddressNibbles = 5;

}  // namespace

ChipDriver::ChipDriver(const Tms5220Config& config, IVsmBus* vsm,
                       ISampleSink* sink, ITms5220Observer* observer)
    : sink_(sink), chip_(config, vsm, this, observer) {}

void ChipDriver::SetMarkHandler(
    std::function<void(std::int64_t, std::string_view)> handler) {
  mark_handler_ = std::move(handler);
}

void ChipDriver::Mark(std::string_view name) {
  if (mark_handler_) {
    mark_handler_(chip_.Slot(), name);
  }
}

void ChipDriver::OnSample(std::int64_t index, std::int64_t slot, int code) {
  idle_run_ = code == -1 ? idle_run_ + 1 : 0;
  if (sink_ != nullptr) {
    sink_->OnSample(index, slot, code);
  }
}

void ChipDriver::StepTo(std::int64_t slot) {
  while (chip_.Slot() < slot) {
    chip_.Step();
  }
}

void ChipDriver::FinishCycle() {
  const std::int64_t limit = chip_.Slot() + kMaxWaitSlots;
  while (chip_.HostBusy()) {
    if (chip_.Slot() >= limit) {
      Logger(kLogModuleEmulator)
          ->warn(
              "READY did not go active within 60 s (slot {}); cycle "
              "abandoned",
              chip_.Slot());
      chip_.AbandonHostCycle();
      return;
    }
    chip_.Step();
  }
  // READY went active during the last slot executed; the next operation
  // starts at the following slot.
}

Status ChipDriver::Write(int byte) {
  if (!chip_.Write(byte)) {
    return FailedPreconditionError("host cycle already in progress");
  }
  FinishCycle();
  return OkStatus();
}

StatusOr<int> ChipDriver::Read() {
  if (!chip_.Read()) {
    return FailedPreconditionError("host cycle already in progress");
  }
  FinishCycle();
  return chip_.HostValue();
}

void ChipDriver::UntilIdle(std::int64_t margin) {
  const std::int64_t limit = chip_.Slot() + kMaxWaitSlots;
  while (chip_.IsTalking() || idle_run_ < kIdleSamples) {
    if (chip_.Slot() >= limit) {
      Logger(kLogModuleEmulator)
          ->warn("until_idle gave up after 60 s at slot {}", chip_.Slot());
      break;
    }
    chip_.Step();
  }
  StepTo(chip_.Slot() + margin);
}

void ChipDriver::Boundary(std::int64_t lead) {
  const std::int64_t latency = chip_.Config().cmd_latch;
  std::int64_t boundary = chip_.NextBoundary();
  while (boundary - lead - latency < chip_.Slot()) {
    boundary += Tms5220::kSlotsPerFrame;
  }
  StepTo(boundary - lead - latency);
}

Status ChipDriver::ResetSequence() {
  for (int i = 0; i < kResetPaddingCount; ++i) {
    if (Status status = Write(kResetPadding); !status.IsOk()) {
      return status;
    }
  }
  return Write(kReset);
}

Status ChipDriver::SpeakFromVsm(std::uint32_t address,
                                const SpeakOptions& options) {
  // Let any prior speech finish.
  UntilIdle(0);
  if (options.reset) {
    if (Status status = ResetSequence(); !status.IsOk()) {
      return status;
    }
  }
  for (int i = 0; i < kAddressNibbles; ++i) {
    const auto nibble = static_cast<int>((address >> (4 * i)) & 0xFU);
    if (Status status = Write(kLoadAddress | nibble); !status.IsOk()) {
      return status;
    }
  }
  Boundary(options.speak_at);
  Mark("speak");
  if (Status status = Write(kSpeak); !status.IsOk()) {
    return status;
  }
  UntilIdle(options.margin);
  return OkStatus();
}

Status ChipDriver::SpeakExternal(std::span<const std::uint8_t> stream,
                                 const SpeakOptions& options) {
  UntilIdle(0);
  if (options.reset) {
    if (Status status = ResetSequence(); !status.IsOk()) {
      return status;
    }
  }
  Boundary(options.speak_at);
  Mark("speak_external");
  if (Status status = Write(kSpeakExternal); !status.IsOk()) {
    return status;
  }
  // Fill the FIFO, then top it up by 8 bytes whenever a status read (every
  // 1 ms) shows Buffer Low.
  std::size_t position = 0;
  while (position < stream.size() &&
         position < static_cast<std::size_t>(kFifoFill)) {
    if (Status status = Write(stream[position++]); !status.IsOk()) {
      return status;
    }
  }
  while (position < stream.size()) {
    StepTo(chip_.Slot() + kStatusPollSlots);
    StatusOr<int> status_byte = Read();
    if (!status_byte.IsOk()) {
      return status_byte.GetStatus();
    }
    if ((*status_byte & kBufferLowBit) == 0) {
      continue;
    }
    for (int k = 0; k < kFifoTopUp && position < stream.size(); ++k) {
      if (Status status = Write(stream[position++]); !status.IsOk()) {
        return status;
      }
    }
  }
  UntilIdle(options.margin);
  return OkStatus();
}

}  // namespace speak5220::core::emulator
