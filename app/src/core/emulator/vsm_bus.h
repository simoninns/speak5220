/*
 * File:        vsm_bus.h
 * Module:      emulator
 * Purpose:     The interface between the TMS5220 and its voice synthesis
 *              memory
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_VSM_BUS_H_
#define SPEAK5220_CORE_EMULATOR_VSM_BUS_H_

namespace speak5220::core::emulator {

// The VSM functions the chip issues on M0/M1 (speak5220 datasheet §4).
// Implemented by Tms6100 and mocked in unit tests. Replaces the C
// reference's callback int fn(void*, int func, int nibble).
class IVsmBus {
 public:
  virtual ~IVsmBus() = default;

  // Load Address: one address nibble (ADD8..ADD1, bit 0 = ADD1).
  virtual void Load(int nibble) = 0;
  // Read: returns the next data bit, 0 or 1.
  virtual int Read() = 0;
  // Dummy Read: starts the fetch after a Load Address; no data.
  virtual void Dummy() = 0;
  // Read and Branch: the memory replaces its address with the pointer at
  // its current address.
  virtual void Branch() = 0;
};

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_VSM_BUS_H_
