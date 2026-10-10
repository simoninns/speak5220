/*
 * File:        tms5220.h
 * Module:      emulator
 * Purpose:     Slot-accurate emulator of the TI TMS5220 voice synthesis
 *              processor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_EMULATOR_TMS5220_H_
#define SPEAK5220_CORE_EMULATOR_TMS5220_H_

#include <array>
#include <cstdint>

#include "core/emulator/sample_sink.h"
#include "core/emulator/tms5220_config.h"
#include "core/emulator/tms5220_event.h"
#include "core/emulator/vsm_bus.h"
#include "core/lpc/coding_tables.h"

namespace speak5220::core::emulator {

// The TMS5220, stepped one slot at a time. A slot is one ROMCLK period
// (6.25 us at 640 kHz); a sample period is 20 slots (speak5220 datasheet
// §2, §8.4). Absolute slot 0 is power-up and is slot 0 of a sample period.
//
// Re-implements the reference emulator investigation/emulator/tms5220.c
// as a class. The structure changes, not the arithmetic or the order of
// evaluation: the codes and events are the reference's, sample for sample
// and slot for slot (plan §2.3). Every "DATASHEET GAP" is a field of
// Tms5220Config or a comment here.
//
// The VSM bus, sample sink and observer are borrowed, may be null, and
// must outlive the chip. Not thread-safe: use one chip from one thread.
// Step() does not allocate.
class Tms5220 {
 public:
  static constexpr int kSlotsPerSample = 20;
  static constexpr int kSamplesPerFrame = 200;
  static constexpr int kSlotsPerFrame = kSlotsPerSample * kSamplesPerFrame;

  // Power-up state. Re-implements tms5220_init().
  Tms5220(const Tms5220Config& config, IVsmBus* vsm, ISampleSink* sink,
          ITms5220Observer* observer);

  Tms5220(const Tms5220&) = delete;
  Tms5220& operator=(const Tms5220&) = delete;
  ~Tms5220() = default;

  // Sets the frame-clock phase of power-up: `sample` is the position
  // (0..199) in the 200-sample frame cycle of the sample period that
  // starts at slot 0; 0 = the period in which a frame would be read (PC 12
  // of IP 7). Call before the first Step(). Re-implements
  // tms5220_set_phase().
  void SetPhase(int sample);

  // Advances one slot. Re-implements tms5220_step().
  void Step();

  // Host cycles. A cycle starts at the current slot, before it is stepped.
  // Only one cycle may be in progress: these return false if one is. Step
  // until HostBusy() is false; READY has then gone active.
  // Re-implement tms5220_write() and tms5220_read().
  bool Write(int byte);
  bool Read();
  // True while READY is inactive.
  bool HostBusy() const { return host_active_; }
  // The value of the last completed cycle (a read's result).
  int HostValue() const { return host_value_; }
  // Gives up the cycle in progress, as a host that times out would.
  void AbandonHostCycle() { host_active_ = false; }

  // The status byte as a read would see it (§5): TS, BL, BE, then 0x1F.
  int StatusByte() const;
  // TS set, or speech running, pending or about to end.
  bool IsTalking() const;
  bool InterruptActive() const { return interrupt_; }
  // The I/O code of the current sample period.
  int Code() const { return code_; }
  // The next slot to be executed.
  std::int64_t Slot() const { return slot_; }
  // The slot at or after the current one at which a VSM frame read would
  // start. Re-implements tms5220_next_boundary().
  std::int64_t NextBoundary() const;
  // The lattice's output stage u1 (§8.8); -9 when idle.
  std::int32_t LatticeU1() const { return u_[1]; }
  const Tms5220Config& Config() const { return config_; }

 private:
  static constexpr int kQueueCapacity = 64;
  static constexpr int kFifoCapacity = 16;
  static constexpr int kParameters = lpc::kParameterCount;

  // Host-side actions scheduled for a slot.
  enum class Action {
    kLoad,
    kDummy,
    kBranch,
    kReadByteBit,
    kTalkStatusSet,
    kNoiseRelease
  };
  struct QueuedAction {
    std::int64_t slot = 0;
    Action action = Action::kLoad;
    int argument = 0;
  };
  // A parameter value on its way to the filter (§8.9, step 1).
  struct QueuedWrite {
    std::int64_t due = 0;
    int parameter = 0;
    int value = 0;
  };
  // A frame being read, field by field (§8.1).
  struct FrameReader {
    int energy = 0;
    int repeat = 0;
    int pitch = 0;
    std::array<int, lpc::kKCount> k = {};
    int bits = 0;
    int field = 0;
    int field_bits = 0;
    int accumulator = 0;
    bool done = false;
  };

  // Events.
  void Emit(const Tms5220Event& event);
  void SpeechEvent(Milestone milestone, int a, int b);
  int VsmCall(VsmFunction function, int nibble);
  bool& FlagRef(StatusFlag flag);
  void SetFlag(StatusFlag flag, bool state);

  // Scheduling.
  void Queue(std::int64_t slot, Action action, int argument);
  std::uint32_t NoiseHoldState() const;
  void RunQueue();

  // FIFO (§5).
  void FifoPurge();
  bool FifoPopBit(int& bit);
  void FifoWrite(int byte);

  // Frame input (§4, §5, §8.1).
  static int FieldWidth(int field);
  static void FrameFeed(FrameReader& frame, int bit);
  void FrameFinished(bool truncated);
  void FrameReadStart();
  void FrameReadSlot();

  // The reference algorithm (§8.9).
  int Target(int parameter) const;
  void QueueWrite(std::int64_t due, int parameter, int value);
  void ApplyWrites();
  void LoadFrame();
  void CheckStart();
  void PeriodStart();

  // Commands (§6) and host cycles (§3).
  void ReadByteStart(std::int64_t ref, std::int64_t first_vsm);
  void ExecuteCommand(int byte, std::int64_t ref, std::int64_t first_vsm);
  void DoReset(std::int64_t latch);
  void LatchCommand(int byte, std::int64_t latch);
  void TalkStatusCleared();
  void HostDone(int value);
  void HostProgress();

  // Members are grouped by size (64-bit, 32-bit, flags) to keep the object
  // free of padding; the comments name the datasheet section each serves.
  IVsmBus* vsm_;
  ISampleSink* sink_;
  ITms5220Observer* observer_;

  // Counters (§7, §8.5).
  std::int64_t slot_ = 0;
  std::int64_t period_ = 0;
  // Host interface (§3, §5, §6).
  std::int64_t host_start_ = 0;
  std::int64_t busy_until_ = -1000;      // no new command latched before
  std::int64_t ready_min_ = -1000;       // a command's READY not before
  std::int64_t read_byte_ready_at_ = 0;  // the 8th bit + 4 slots
  std::int64_t ts_clear_at_ = -1;        // when Talk Status clears
  // Speech control, host side.
  std::int64_t pending_latch_ = 0;
  std::int64_t external_latch_ = 0;
  std::int64_t external_ready_at_ = 0;
  // Frame input.
  std::int64_t frame_next_ = 0;
  std::int64_t frame_start_ = 0;

  std::array<QueuedAction, kQueueCapacity> queue_ = {};
  std::array<QueuedWrite, kQueueCapacity> writes_ = {};  // §8.9 step 1
  FrameReader frame_;                                    // the frame being read
  FrameReader new_frame_;  // read, waiting to load at PC 12 of IP 0
  Tms5220Config config_;

  // Synthesis state, named as in §8.9 "Reference algorithm (exact)".
  std::array<int, kParameters> index_ = {};    // coded indices
  std::array<int, kParameters> current_ = {};  // interpolator registers
  std::array<int, kParameters> seen_ = {};     // values the filter uses
  // Lattice (§8.8).
  std::array<std::int32_t, 12> u_ = {};
  std::array<std::int32_t, 11> b_ = {};
  int code_ = -1;

  int frame_pos_ = 0;  // 0..199; 0 = PC 12 of IP 7 (VSM read)
  int synth_offset_ = 0;
  int synth_pos_ = 0;
  HostCycle host_kind_ = HostCycle::kStatus;
  int host_value_ = 0;
  int held_command_ = -1;  // command held during speech, or -1
  int read_byte_bits_ = 0;
  int read_byte_value_ = 0;
  int queue_size_ = 0;
  int write_count_ = 0;
  int utterance_frames_ = 0;
  int loads_ = 0;
  int count_ = 0;            // pitch counter
  std::uint32_t noise_ = 0;  // noise generator (§7, §8.6)
  int fifo_count_ = 0;
  int fifo_bit_ = 0;
  // FIFO (§5): 16 bytes, read LSB first.
  std::array<std::uint8_t, kFifoCapacity> fifo_ = {};

  // Host interface flags.
  bool host_active_ = false;
  bool host_write_ = false;
  bool need_dummy_ = false;  // a Load Address since the last read
  bool data_mode_ = false;   // the next read returns Read Byte data
  bool read_byte_done_ = false;
  // Status flags.
  bool talk_status_ = false;
  bool buffer_low_ = true;  // original §7: the FIFO is empty at power-up
  bool buffer_empty_ = true;
  bool interrupt_ = false;
  // Speech control and frame input flags.
  bool pending_speak_ = false;
  bool external_active_ = false;
  bool external_ready_ = false;
  bool speak_external_ = false;
  bool frame_active_ = false;
  bool new_frame_valid_ = false;
  bool noise_held_ = true;
  // Synthesis flags (§8.9).
  bool talking_ = false;
  bool requested_ = false;
  bool zeroing_ = false;   // Z
  bool unvoiced_ = false;  // U
  bool last_unvoiced_ = false;
  bool inhibit_ = false;
  bool old_silence_ = false;   // oldS
  bool old_unvoiced_ = false;  // oldU
  bool old_unvoiced_previous_ = false;
  bool hold_ = false;
};

}  // namespace speak5220::core::emulator

#endif  // SPEAK5220_CORE_EMULATOR_TMS5220_H_
