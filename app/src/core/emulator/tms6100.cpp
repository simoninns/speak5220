/*
 * File:        tms6100.cpp
 * Module:      emulator
 * Purpose:     Minimal TMS6100 voice synthesis memory, enough to drive the
 *              TMS5220 emulator
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/tms6100.h"

#include <cstddef>
#include <cstdint>
#include <span>

namespace speak5220::core::emulator {
namespace {

constexpr std::uint32_t kAddressMask =
    static_cast<std::uint32_t>(Tms6100::kMemorySize - 1);
// §4: Read and Branch replaces the low 14 bits of the address.
constexpr std::uint32_t kBranchMask = 0x3FFF;
// The bit position of the top nibble of a 20-bit address.
constexpr int kTopNibbleShift = 16;

}  // namespace

Tms6100::Tms6100() : memory_(kMemorySize, 0) {}

void Tms6100::LoadMemory(std::span<const std::uint8_t> data,
                         std::uint32_t address) {
  for (std::size_t i = 0; i < data.size(); ++i) {
    memory_[(address + i) & kAddressMask] = data[i];
  }
}

void Tms6100::Load(int nibble) {
  // §4: nibbles shift in least significant first; five give a full 20-bit
  // address, fewer leave the earlier address shifted, a sixth shifts it
  // on.
  const auto value = static_cast<std::uint32_t>(nibble) & 0xFU;
  address_ = ((address_ >> 4) | (value << kTopNibbleShift)) & kAddressMask;
  bit_ = 0;
}

int Tms6100::Read() {
  // §6: the chip reads each byte LSB first.
  const int bit = (memory_[address_] >> bit_) & 1;
  if (++bit_ == 8) {
    bit_ = 0;
    address_ = (address_ + 1) & kAddressMask;
  }
  return bit;
}

void Tms6100::Dummy() {
  // Original §4: the Dummy Read resets the load pointer and starts the
  // fetch; no data is transferred.
  bit_ = 0;
}

void Tms6100::Branch() {
  // §4: the VSM replaces its address with the two bytes at its current
  // address: the first byte read is the low byte, and the low 14 bits of
  // the word replace the low 14 bits of the address.
  const std::uint32_t low = memory_[address_];
  const std::uint32_t high = memory_[(address_ + 1) & kAddressMask];
  const std::uint32_t word = (high << 8) | low;
  address_ = (address_ & ~kBranchMask) | (word & kBranchMask);
  bit_ = 0;
}

}  // namespace speak5220::core::emulator
