/*
 * File:        tms5220_event.h
 * Module:      emulator
 * Purpose:     The events the TMS5220 emulator reports, and the interface
 *              that observes them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_TMS5220_EVENT_H_
#define SPEAK5220_CORE_EMULATOR_TMS5220_EVENT_H_

#include <cstdint>
#include <string_view>

namespace speak5220::core::emulator {

// What an event reports. Re-implements tms5220.h tms5220_event_type.
enum class EventType {
  kVsm,     // a VSM function: function, argument (nibble or bit read)
  kHost,    // a completed host cycle: host, value, start, ready
  kFlag,    // a change of TS, BL, BE or INT: flag, state
  kSpeech,  // an internal milestone (informative): milestone, a, b
};

// VSM functions (speak5220 datasheet §4).
enum class VsmFunction { kLoad, kRead, kDummy, kBranch };

// Kinds of completed host cycle.
enum class HostCycle {
  kCommand,  // write latched into the command register
  kFifo,     // write routed to the FIFO (Speak External)
  kStatus,   // read returning the status register
  kData,     // read returning the Read Byte data register
};

// The status flags and the interrupt line (§3, §5).
enum class StatusFlag { kTalkStatus, kBufferLow, kBufferEmpty, kInterrupt };

// Internal milestones, named as in the C reference's event log.
enum class Milestone {
  kStartSpeak,      // a Speak took effect (a: frame clock position)
  kStartExternal,   // a Speak External took effect
  kFrame,           // a frame was read in full (a: bits, b: energy index)
  kFrameTruncated,  // Speak External ran out of data mid-frame
  kLoad,            // a frame loaded in PC 12 of IP 0 (a: energy, b: inhibit)
  kEnd,             // speech ended
  kHeld,            // a command held during speech (a: byte)
  kHeldExec,        // a held command released when TS cleared (a: byte)
  kIgnored,         // a Speak ignored during speech (a: byte)
  kResetHalt,       // a Reset halted speech
  kFifoOverflow,    // a byte written to a full FIFO (a: byte, b: count)
};

// "start_speak", "frame", ... as in the C reference's event log.
std::string_view MilestoneName(Milestone milestone);

// One event. Only the fields of its type are meaningful. Re-implements
// tms5220.h tms5220_event.
struct Tms5220Event {
  EventType type = EventType::kSpeech;
  std::int64_t slot = 0;  // when it happened (host: the strobe slot)
  // kVsm
  VsmFunction function = VsmFunction::kLoad;
  int argument = 0;  // Load: the nibble; Read: the bit; otherwise -1
  // kHost
  HostCycle host = HostCycle::kCommand;
  int value = 0;
  std::int64_t start = 0;  // the strobe slot
  std::int64_t ready = 0;  // the slot READY went active
  // kFlag
  StatusFlag flag = StatusFlag::kTalkStatus;
  bool state = false;  // set, or INT active
  // kSpeech
  Milestone milestone = Milestone::kStartSpeak;
  int a = 0;
  int b = 0;
};

// Observes the chip's events. Called on the thread that steps the chip,
// in the order the events happen. Replaces the C reference's event
// callback.
class ITms5220Observer {
 public:
  virtual ~ITms5220Observer() = default;
  virtual void OnEvent(const Tms5220Event& event) = 0;
};

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_TMS5220_EVENT_H_
