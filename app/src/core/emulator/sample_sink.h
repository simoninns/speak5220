/*
 * File:        sample_sink.h
 * Module:      emulator
 * Purpose:     Receives the TMS5220's output code once per sample period
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_SAMPLE_SINK_H_
#define SPEAK5220_CORE_EMULATOR_SAMPLE_SINK_H_

#include <cstdint>

namespace speak5220::core::emulator {

// Receives each sample period's output. Replaces the C reference's sample
// callback. Called on the thread that steps the chip.
class ISampleSink {
 public:
  virtual ~ISampleSink() = default;

  // Called once per sample period, at its slot 0, with the 10-bit I/O code
  // sent in slots 2..11 of that period (speak5220 datasheet §8.4). `index`
  // is the sample period number since power-up; `slot` the absolute slot at
  // which the period began.
  virtual void OnSample(std::int64_t index, std::int64_t slot, int code) = 0;
};

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_SAMPLE_SINK_H_
