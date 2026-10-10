/*
 * File:        tms5220_config.h
 * Module:      emulator
 * Purpose:     The timing choices the datasheet leaves open, one field per
 *              "DATASHEET GAP" of the emulator
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_TMS5220_CONFIG_H_
#define SPEAK5220_CORE_EMULATOR_TMS5220_CONFIG_H_

#include <string_view>

#include "core/util/status.h"

namespace speak5220::core::emulator {

// Timing choices that the datasheet does not pin down to the slot, in
// slots of 6.25 us. Each default is the choice listed in the emulator's
// implementation choices (docs/technical/emulator/choices.md).
// Re-implements tms5220.h tms5220_config and tms5220_default_config().
struct Tms5220Config {
  // UNMEASURED (§3 "Not measured yet"): READY latencies as whole slots,
  // chosen from Table 3-1 (status 11.5-17.4 us, command 18.4-23.9 us,
  // FIFO 14.7-20.7 us).
  int cmd_latch = 4;   // write strobe to command latched: 25.0 us
  int fifo_latch = 3;  // write strobe to FIFO byte latched: 18.75 us
  int read_ready = 3;  // read strobe to READY active: 18.75 us
  // §4 gives the Load Address function "about 35-45 us after the write":
  // latch (4) + 2 = 37.5 us, and the Read Byte's Dummy Read 2 slots after
  // the latch. UNMEASURED (§3): the same delay for every other command's
  // first VSM function, the Speak's Dummy Read included.
  int vsm_delay = 2;
  // §4: VSM functions two slots (12.5 us) apart.
  int vsm_spacing = 2;
  // §3 "When READY returns": READY after Read and Branch goes active at the
  // first slot 15 more than 59 and at most 63 slots after the Branch.
  // DATASHEET GAP: the exact limit is not settled; 60, the "more than 59
  // slots" of the text.
  int branch_ready = 60;
  // §8.5, §8.9: a Speak starts at the first frame-read sample whose slot 17
  // is at least this many slots after the latch.
  int speak_lead = 52;
  // UNMEASURED (§3): slots from Speak latched to Talk Status set (later
  // than 22 us).
  int ts_set_delay = 2;
  // §4, §8.5: TS clears 24.87 ms after the stop frame is read (slot 16 of
  // its sample 199, 3979 slots after the first bit in slot 17).
  int ts_clear_stop = 3979;
  // §4: and 0.09 ms after a stop code that is the first frame.
  int ts_clear_first_stop = 14;
  // §7: the noise register first steps 6 slots (Speak) or 1 slot (Speak
  // External) after the latch.
  int noise_rel_speak = 6;
  int noise_rel_se = 1;
  // §7: Reset holds the noise register at 0x1FFF advanced this many steps.
  // DATASHEET GAP: the power-up noise state is not given.
  int noise_hold_steps = 0;
  // §8.5 "Speak External": sample 0 two sample periods before the VSM read
  // point; the frame is taken in sample 198 of the frame clock.
  int se_shift = 2;
  // §4: the slot of the sample period in which a frame read starts.
  int frame_read_slot = 17;
  // §4: the Load Address nibble issued by Reset (observed 8 for a Reset
  // during speech; used for every Reset).
  int reset_nibble = 8;
};

// Sets the field called `name` (a C reference field name, such as
// "se_shift") to `value`. Fails for an unknown name. Used by scenario
// scripts (docs/technical/emulator/command-line.md, `set`).
Status SetConfigParameter(Tms5220Config& config, std::string_view name,
                          int value);

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_TMS5220_CONFIG_H_
