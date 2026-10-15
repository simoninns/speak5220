/*
 * File:        tms6100.h
 * Module:      emulator
 * Purpose:     Minimal TMS6100 voice synthesis memory, enough to drive the
 *              TMS5220 emulator
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_TMS6100_H_
#define SPEAK5220_CORE_EMULATOR_TMS6100_H_

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/emulator/vsm_bus.h"

namespace speak5220::core::emulator {

// A VSM with a 20-bit address space (speak5220 datasheet §4 and the
// original manual's VSM description). Bytes never loaded read 0x00.
// Re-implements the retired C emulator's tms6100.c. Not thread-safe; it is
// driven by one chip.
class Tms6100 : public IVsmBus {
 public:
  // 20-bit address space, 1 MB.
  static constexpr std::size_t kMemorySize = std::size_t{1} << 20;

  Tms6100();

  // Copies `data` into memory from `address`, wrapping at the top.
  // Re-implements tms6100_load_mem().
  void LoadMemory(std::span<const std::uint8_t> data, std::uint32_t address);

  // The address register, for tests.
  std::uint32_t Address() const { return address_; }

  // IVsmBus, re-implementing tms6100_func().
  void Load(int nibble) override;
  int Read() override;
  void Dummy() override;
  void Branch() override;

 private:
  std::vector<std::uint8_t> memory_;
  std::uint32_t address_ = 0;
  int bit_ = 0;  // the next bit of the current byte, 0 = LSB
};

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_TMS6100_H_
