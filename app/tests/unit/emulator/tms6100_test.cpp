/*
 * File:        tms6100_test.cpp
 * Module:      emulator tests
 * Purpose:     Unit tests of the TMS6100 model: address loading, bit order,
 *              Dummy Read and Read and Branch
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/tms6100.h"

#include <array>
#include <cstdint>

#include "gtest/gtest.h"

namespace speak5220::core::emulator {
namespace {

void LoadAddress(Tms6100& vsm, std::uint32_t address) {
  for (int i = 0; i < 5; ++i) {
    vsm.Load(static_cast<int>((address >> (4 * i)) & 0xF));
  }
}

int ReadByte(Tms6100& vsm) {
  int value = 0;
  for (int i = 0; i < 8; ++i) {
    value |= vsm.Read() << i;
  }
  return value;
}

TEST(Tms6100Test, FiveNibblesLoadAddressLowFirst) {
  Tms6100 vsm;
  LoadAddress(vsm, 0x3C437);
  EXPECT_EQ(vsm.Address(), 0x3C437U);
}

TEST(Tms6100Test, FewerNibblesLeaveEarlierAddressShifted) {
  // §4: a sixth nibble shifts the address on.
  Tms6100 vsm;
  LoadAddress(vsm, 0x12345);
  vsm.Load(0x6);
  EXPECT_EQ(vsm.Address(), 0x61234U);
}

TEST(Tms6100Test, ReadsEachByteLsbFirstThenAdvances) {
  Tms6100 vsm;
  const std::array<std::uint8_t, 2> data = {0x23, 0x9B};
  vsm.LoadMemory(data, 0x100);
  LoadAddress(vsm, 0x100);
  EXPECT_EQ(vsm.Read(), 1);  // 0x23 = 0010 0011: bit 0 first
  EXPECT_EQ(vsm.Read(), 1);
  EXPECT_EQ(vsm.Read(), 0);
  vsm.Dummy();  // restarts the byte
  EXPECT_EQ(ReadByte(vsm), 0x23);
  EXPECT_EQ(ReadByte(vsm), 0x9B);
  EXPECT_EQ(vsm.Address(), 0x102U);
}

TEST(Tms6100Test, UnloadedMemoryReadsZero) {
  Tms6100 vsm;
  LoadAddress(vsm, 0xABCDE);
  EXPECT_EQ(ReadByte(vsm), 0);
}

TEST(Tms6100Test, LoadingWrapsAtTopOfMemory) {
  Tms6100 vsm;
  const std::array<std::uint8_t, 2> data = {0x11, 0x22};
  vsm.LoadMemory(data, 0xFFFFF);
  LoadAddress(vsm, 0);
  EXPECT_EQ(ReadByte(vsm), 0x22);
}

TEST(Tms6100Test, BranchReplacesLowFourteenBits) {
  // §4: the low byte first; the word's low 14 bits replace the address's.
  Tms6100 vsm;
  const std::array<std::uint8_t, 2> pointer = {0xB6, 0xCB};
  vsm.LoadMemory(pointer, 0x3C140);
  LoadAddress(vsm, 0x3C140);
  vsm.Branch();
  EXPECT_EQ(vsm.Address(), 0x3C000U | (0xCBB6U & 0x3FFFU));
}

}  // namespace
}  // namespace speak5220::core::emulator
