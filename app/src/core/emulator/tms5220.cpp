/*
 * File:        tms5220.cpp
 * Module:      emulator
 * Purpose:     Slot-accurate emulator of the TI TMS5220 voice synthesis
 *              processor
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/tms5220.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

#include "core/emulator/chip_arithmetic.h"
#include "core/lpc/coding_tables.h"

namespace speak5220::core::emulator {
namespace {

using std::int64_t;

// Energy indices with a special meaning (§8.1).
constexpr int kSilenceEnergy = 0;
constexpr int kStopEnergy = 15;

// Command codes, bits 6..4 of a command byte (§6).
constexpr int kCommandReadByte = 1;
constexpr int kCommandReadAndBranch = 3;
constexpr int kCommandLoadAddress = 4;
constexpr int kCommandSpeak = 5;
constexpr int kCommandSpeakExternal = 6;
constexpr int kCommandReset = 7;

// §5: BL and TS change with the 9th byte of the FIFO.
constexpr int kFifoLowThreshold = 9;
// §4 "When Read Byte reads": the slot of a sample period at which the eight
// bits start (13, 15, ..., 19, 1, ..., 7).
constexpr int kReadByteFirstSlot = 13;
// §4: after a Dummy Read, the first slot 13 at least this many slots on.
constexpr int kReadByteAfterDummy = 22;
// §3: the data read's READY goes active 4 slots after the eighth bit.
constexpr int kReadByteReadyDelay = 4;
// §3 "When READY returns": READY after Read and Branch is at slot 15.
constexpr int kBranchReadySlot = 15;

// §4 "The rest of the frame": slots from the frame's first bit to the first
// bit of each parameter (energy, repeat and pitch, K1..K10). Within a
// parameter the bits are two slots apart. The table gives repeat and pitch
// as one parameter of 1 + 6 bits, so the pitch follows the repeat bit.
// K10's last bit (slot 446) still comes before the frame is loaded, in PC
// 12 of IP 0, 25 samples after the read sample (483 slots after the first
// bit).
constexpr std::array<int, 12> kParameterSlot = {0,   34,  78,  118, 160, 200,
                                                240, 280, 320, 362, 402, 442};

// §8.9: samples from a parameter's computation to the filter seeing it.
constexpr std::array<int, 12> kFilterDelay = {2, 3, 1, 1, 1, 1,
                                              1, 1, 1, 1, 1, 1};
// §8.7: the interpolation shift of each interpolation period.
constexpr std::array<int, 8> kInterpolationShift = {0, 3, 3, 3, 2, 2, 1, 1};

// §8.6: the excitation of an unvoiced sample, in units of 1/128.
constexpr std::int32_t kNoiseExcitation = 64;
// §8.8: the excitation is scaled by 64 before the energy multiply.
constexpr std::int32_t kExcitationScale = 64;
// §8.8: the code is u1 shifted right by 4.
constexpr int kCodeShift = 4;
// §8.1, Table 8-7: a pitch counter above 51 reads location 51.
constexpr int kLastChirp = 51;

bool IsSilenceIndex(const std::array<int, lpc::kParameterCount>& index) {
  return index[0] == kSilenceEnergy || index[0] == kStopEnergy;
}
bool IsUnvoicedIndex(const std::array<int, lpc::kParameterCount>& index) {
  return index[1] == 0;
}

}  // namespace

std::string_view MilestoneName(Milestone milestone) {
  switch (milestone) {
    case Milestone::kStartSpeak:
      return "start_speak";
    case Milestone::kStartExternal:
      return "start_external";
    case Milestone::kFrame:
      return "frame";
    case Milestone::kFrameTruncated:
      return "frame_truncated";
    case Milestone::kLoad:
      return "load";
    case Milestone::kEnd:
      return "end";
    case Milestone::kHeld:
      return "held";
    case Milestone::kHeldExec:
      return "held_exec";
    case Milestone::kIgnored:
      return "ignored";
    case Milestone::kResetHalt:
      return "reset_halt";
    case Milestone::kFifoOverflow:
      return "fifo_overflow";
  }
  return "unknown";
}

// ---------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------

Tms5220::Tms5220(const Tms5220Config& config, IVsmBus* vsm, ISampleSink* sink,
                 ITms5220Observer* observer)
    : vsm_(vsm), sink_(sink), observer_(observer), config_(config) {
  // §8.9 "Not known for this path": the power-up state is not measured;
  // every index, register and flag at 0 and the filter at its idle state
  // (u11 = +1, each stage one less, u1 = -9; b_i as left by that state).
  // UNMEASURED (§3): what the power-up clear issues to the VSM and its
  // effect on INT: nothing is issued, INT inactive. The FIFO is empty
  // (original §7: BL and BE set). The noise register is held at 0x1FFF as
  // after Reset. DATASHEET GAP: the power-up noise state is not given.
  for (std::size_t i = 1; i <= 11; ++i) {
    u_[i] = static_cast<std::int32_t>(i) - 10;
  }
  for (std::size_t i = 1; i <= 10; ++i) {
    b_[i] = static_cast<std::int32_t>(i) - 10;
  }
  noise_ = NoiseHoldState();
}

void Tms5220::SetPhase(int sample) {
  sample %= kSamplesPerFrame;
  if (sample < 0) {
    sample += kSamplesPerFrame;
  }
  frame_pos_ = sample;
}

// ---------------------------------------------------------------------
// Events
// ---------------------------------------------------------------------

void Tms5220::Emit(const Tms5220Event& event) {
  if (observer_ != nullptr) {
    observer_->OnEvent(event);
  }
}

void Tms5220::SpeechEvent(Milestone milestone, int a, int b) {
  if (observer_ == nullptr) {
    return;
  }
  Tms5220Event event;
  event.type = EventType::kSpeech;
  event.slot = slot_;
  event.milestone = milestone;
  event.a = a;
  event.b = b;
  Emit(event);
}

int Tms5220::VsmCall(VsmFunction function, int nibble) {
  int result = 0;
  if (vsm_ != nullptr) {
    switch (function) {
      case VsmFunction::kLoad:
        vsm_->Load(nibble);
        break;
      case VsmFunction::kRead:
        result = vsm_->Read() & 1;
        break;
      case VsmFunction::kDummy:
        vsm_->Dummy();
        break;
      case VsmFunction::kBranch:
        vsm_->Branch();
        break;
    }
  }
  if (observer_ != nullptr) {
    Tms5220Event event;
    event.type = EventType::kVsm;
    event.slot = slot_;
    event.function = function;
    event.argument = function == VsmFunction::kLoad   ? nibble
                     : function == VsmFunction::kRead ? result
                                                      : -1;
    Emit(event);
  }
  return result;
}

bool& Tms5220::FlagRef(StatusFlag flag) {
  switch (flag) {
    case StatusFlag::kTalkStatus:
      return talk_status_;
    case StatusFlag::kBufferLow:
      return buffer_low_;
    case StatusFlag::kBufferEmpty:
      return buffer_empty_;
    case StatusFlag::kInterrupt:
      return interrupt_;
  }
  return interrupt_;
}

void Tms5220::SetFlag(StatusFlag flag, bool state) {
  bool& value = FlagRef(flag);
  const bool old = value;
  if (old == state) {
    return;
  }
  value = state;
  if (observer_ != nullptr) {
    Tms5220Event event;
    event.type = EventType::kFlag;
    event.slot = slot_;
    event.flag = flag;
    event.state = state;
    Emit(event);
  }
  // §3 (Correction): INT goes active when TS goes clear and when BL goes
  // set, not when BE goes set.
  if (flag == StatusFlag::kTalkStatus && old && !state) {
    SetFlag(StatusFlag::kInterrupt, true);
  }
  if (flag == StatusFlag::kBufferLow && !old && state) {
    SetFlag(StatusFlag::kInterrupt, true);
  }
}

// ---------------------------------------------------------------------
// Scheduled host-side actions
// ---------------------------------------------------------------------

void Tms5220::Queue(int64_t slot, Action action, int argument) {
  if (queue_size_ < kQueueCapacity) {
    queue_[static_cast<std::size_t>(queue_size_)] = {
        .slot = slot, .action = action, .argument = argument};
    ++queue_size_;
  }
}

std::uint32_t Tms5220::NoiseHoldState() const {
  std::uint32_t n = kNoiseResetState;
  for (int i = 0; i < config_.noise_hold_steps; ++i) {
    n = NoiseStep(n);
  }
  return n;
}

void Tms5220::RunQueue() {
  // Entries due are taken in the reference's order: a due entry is replaced
  // by the last one, which is examined next.
  int i = 0;
  while (i < queue_size_) {
    const QueuedAction entry = queue_[static_cast<std::size_t>(i)];
    if (entry.slot > slot_) {
      ++i;
      continue;
    }
    --queue_size_;
    queue_[static_cast<std::size_t>(i)] =
        queue_[static_cast<std::size_t>(queue_size_)];
    switch (entry.action) {
      case Action::kLoad:
        VsmCall(VsmFunction::kLoad, entry.argument);
        break;
      case Action::kDummy:
        VsmCall(VsmFunction::kDummy, 0);
        break;
      case Action::kBranch:
        VsmCall(VsmFunction::kBranch, 0);
        break;
      case Action::kReadByteBit: {
        const int bit = VsmCall(VsmFunction::kRead, 0);
        // §6: the first bit read appears on D0, TI's MSB (host bit 7).
        read_byte_value_ |= bit << (7 - read_byte_bits_);
        if (++read_byte_bits_ == 8) {
          read_byte_done_ = true;
          read_byte_ready_at_ = slot_ + kReadByteReadyDelay;
        }
        break;
      }
      case Action::kTalkStatusSet:
        if (pending_speak_) {
          SetFlag(StatusFlag::kTalkStatus, true);
        }
        break;
      case Action::kNoiseRelease:
        noise_held_ = false;
        break;
    }
  }
}

// ---------------------------------------------------------------------
// FIFO (§5)
// ---------------------------------------------------------------------

void Tms5220::FifoPurge() {
  fifo_count_ = 0;
  fifo_bit_ = 0;
  SetFlag(StatusFlag::kBufferLow, true);
  SetFlag(StatusFlag::kBufferEmpty, true);
}

bool Tms5220::FifoPopBit(int& bit) {
  if (fifo_count_ == 0) {
    return false;
  }
  bit = (fifo_[0] >> fifo_bit_) & 1;  // LSB first (original §5.2)
  if (++fifo_bit_ == 8) {
    for (int i = 1; i < fifo_count_; ++i) {
      fifo_[static_cast<std::size_t>(i - 1)] =
          fifo_[static_cast<std::size_t>(i)];
    }
    --fifo_count_;
    fifo_bit_ = 0;
  }
  return true;
}

void Tms5220::FifoWrite(int byte) {
  if (fifo_count_ >= kFifoCapacity) {
    // UNMEASURED (§3): a 17th byte written to a full FIFO: discarded.
    SpeechEvent(Milestone::kFifoOverflow, byte, fifo_count_);
    return;
  }
  fifo_[static_cast<std::size_t>(fifo_count_++)] =
      static_cast<std::uint8_t>(byte);
  SetFlag(StatusFlag::kBufferEmpty, false);  // §5: BE clears with byte 1
  if (fifo_count_ >= kFifoLowThreshold) {
    SetFlag(StatusFlag::kBufferLow, false);  // §5: BL clears with byte 9
    if (external_active_ && !external_ready_) {
      external_ready_ = true;  // §5: TS sets at once with the 9th
      external_ready_at_ = slot_;
      SetFlag(StatusFlag::kTalkStatus, true);
    }
  }
}

// ---------------------------------------------------------------------
// Frame parsing (§8.1)
// ---------------------------------------------------------------------

int Tms5220::FieldWidth(int field) {
  if (field == 0) {
    return 4;
  }
  if (field == 1) {
    return 1;
  }
  if (field == 2) {
    return 6;
  }
  return lpc::kKBits[static_cast<std::size_t>(field - 3)];
}

// Feeds one bit, most significant bit of each field first.
void Tms5220::FrameFeed(FrameReader& frame, int bit) {
  frame.accumulator = (frame.accumulator << 1) | (bit & 1);
  ++frame.field_bits;
  ++frame.bits;
  if (frame.field_bits < FieldWidth(frame.field)) {
    return;
  }
  switch (frame.field) {
    case 0:
      frame.energy = frame.accumulator;
      if (frame.energy == kSilenceEnergy || frame.energy == kStopEnergy) {
        frame.done = true;
      }
      break;
    case 1:
      frame.repeat = frame.accumulator;
      break;
    case 2:
      frame.pitch = frame.accumulator;
      if (frame.repeat != 0) {
        frame.done = true;
      }
      break;
    default:
      frame.k[static_cast<std::size_t>(frame.field - 3)] = frame.accumulator;
      if (frame.field == 6 && frame.pitch == 0) {
        frame.done = true;
      }
      if (frame.field == 12) {
        frame.done = true;
      }
      break;
  }
  ++frame.field;
  frame.field_bits = 0;
  frame.accumulator = 0;
}

// A frame has been read in full, or (Speak External) ran out of data.
void Tms5220::FrameFinished(bool truncated) {
  frame_active_ = false;
  SpeechEvent(truncated ? Milestone::kFrameTruncated : Milestone::kFrame,
              frame_.bits, frame_.energy);
  if (truncated) {
    // §8.9: "a stop code if a Speak External ran out of data".
    frame_ = {};
    frame_.energy = kStopEnergy;
  }
  if (frame_.energy == kStopEnergy) {
    // §4: TS clears 0.09 ms after a stop code that is the first frame,
    // else 24.87 ms after the stop frame is read. DATASHEET GAP: for a
    // first-frame stop, §8.9 still loads the frame and keeps talking to
    // sample 200 (clearing Z and U); implemented literally.
    if (utterance_frames_ == 0 && !truncated) {
      ts_clear_at_ = frame_start_ + config_.ts_clear_first_stop;
    } else {
      ts_clear_at_ = frame_start_ + config_.ts_clear_stop;
    }
  }
  new_frame_ = frame_;
  new_frame_valid_ = true;
  ++utterance_frames_;
}

void Tms5220::FrameReadStart() {
  frame_ = {};
  frame_active_ = true;
  frame_start_ = period_ * kSlotsPerSample + config_.frame_read_slot;
  frame_next_ = frame_start_;
}

void Tms5220::FrameReadSlot() {
  int bit = 0;
  if (!speak_external_) {
    // §4: one bit per Read function, two slots apart within a parameter;
    // each parameter starts at its offset from the frame's first bit. A
    // frame that ends early stops after its last field.
    bit = VsmCall(VsmFunction::kRead, 0);
    FrameFeed(frame_, bit);
    if (frame_.done) {
      FrameFinished(false);
    } else if (frame_.field_bits > 0 || frame_.field == 2) {
      frame_next_ += config_.vsm_spacing;
    } else {
      const int parameter = frame_.field <= 1 ? frame_.field : frame_.field - 1;
      frame_next_ =
          frame_start_ + kParameterSlot[static_cast<std::size_t>(parameter)];
    }
    return;
  }
  // §5: bits leave the FIFO as the chip reads each frame. DATASHEET GAP:
  // §5 says the flags fit the VSM path's 446-slot schedule (§4) better
  // than taking all the bits at once, but leaves the rule (and so the times
  // of BL, BE and TS within a frame) "not yet settled". Kept: all of a
  // frame's bits leave at once, at slot 17 of the (Speak External)
  // frame-read sample, as for the VSM's first bit.
  while (!frame_.done) {
    if (!FifoPopBit(bit)) {
      break;
    }
    FrameFeed(frame_, bit);
  }
  if (!frame_.done) {
    // §5: BE sets when a frame read needs more bits than the FIFO holds.
    // DATASHEET GAP: what is left in the FIFO is not said; it is emptied.
    fifo_count_ = 0;
    fifo_bit_ = 0;
    SetFlag(StatusFlag::kBufferEmpty, true);
    SetFlag(StatusFlag::kBufferLow, true);
    FrameFinished(true);
    return;
  }
  // DATASHEET GAP: §6 says a stream whose stop code ends exactly on a byte
  // boundary finishes with BE set, but §5 defines BE only for a frame that
  // cannot be read in full. BE also sets here when a frame read leaves the
  // FIFO empty.
  if (fifo_count_ == 0) {
    SetFlag(StatusFlag::kBufferEmpty, true);
  }
  // §5: BL sets at the frame read that leaves 8 bytes or fewer.
  if (fifo_count_ < kFifoLowThreshold) {
    SetFlag(StatusFlag::kBufferLow, true);
  }
  FrameFinished(false);
}

// ---------------------------------------------------------------------
// §8.9 reference algorithm
// ---------------------------------------------------------------------

int Tms5220::Target(int parameter) const {
  if (zeroing_ || (unvoiced_ && parameter >= 6)) {
    return 0;
  }
  return lpc::ParameterValue(parameter,
                             index_[static_cast<std::size_t>(parameter)]);
}

void Tms5220::QueueWrite(int64_t due, int parameter, int value) {
  if (due <= period_) {
    seen_[static_cast<std::size_t>(parameter)] = value;  // due now
    return;
  }
  if (write_count_ < kQueueCapacity) {
    writes_[static_cast<std::size_t>(write_count_)] = {
        .due = due, .parameter = parameter, .value = value};
    ++write_count_;
  }
}

void Tms5220::ApplyWrites() {
  int kept = 0;
  for (int j = 0; j < write_count_; ++j) {
    const QueuedWrite write = writes_[static_cast<std::size_t>(j)];
    if (write.due <= period_) {
      seen_[static_cast<std::size_t>(write.parameter)] = write.value;
    } else {
      writes_[static_cast<std::size_t>(kept++)] = write;
    }
  }
  write_count_ = kept;
}

// Loading a frame (§8.9), in PC 12 of IP 0 while talking.
void Tms5220::LoadFrame() {
  const FrameReader& frame = new_frame_;
  const bool silence =
      frame.energy == kSilenceEnergy || frame.energy == kStopEnergy;

  ++loads_;
  if (!(frame.energy == kSilenceEnergy && loads_ == 1)) {
    zeroing_ = false;  // §8.9: clear Z and U
    unvoiced_ = false;
  }
  index_[0] = frame.energy;
  if (frame.energy == kStopEnergy) {
    requested_ = false;
  }
  if (!silence) {
    index_[1] = frame.pitch;
    if (frame.repeat != 0) {
      unvoiced_ = frame.pitch == 0 || last_unvoiced_;
    } else {
      unvoiced_ = frame.pitch == 0;
      last_unvoiced_ = unvoiced_;
      const std::size_t count = frame.pitch != 0 ? 10 : 4;
      for (std::size_t j = 0; j < count; ++j) {
        index_[2 + j] = frame.k[j];
      }
    }
  }
  inhibit_ = (old_unvoiced_ != IsUnvoicedIndex(index_)) ||
             (old_silence_ && !IsSilenceIndex(index_)) ||
             (old_unvoiced_ && IsSilenceIndex(index_));
  new_frame_valid_ = false;
  SpeechEvent(Milestone::kLoad, frame.energy, inhibit_ ? 1 : 0);
}

// A Speak (or Speak External) takes effect at the start of its frame-read
// sample (§8.9).
void Tms5220::CheckStart() {
  const int64_t read_slot = period_ * kSlotsPerSample + config_.frame_read_slot;
  const int external_pos =
      ((kSamplesPerFrame - config_.se_shift) % kSamplesPerFrame +
       kSamplesPerFrame) %
      kSamplesPerFrame;
  if (requested_) {
    return;
  }
  if (pending_speak_ && frame_pos_ == 0 &&
      read_slot - pending_latch_ >= config_.speak_lead) {
    pending_speak_ = false;
    speak_external_ = false;
    synth_offset_ = 0;
  } else if (external_active_ && external_ready_ &&
             frame_pos_ == external_pos &&
             read_slot - external_latch_ >= config_.speak_lead &&
             read_slot - external_ready_at_ >= config_.speak_lead) {
    // UNMEASURED: whether the 52-slot lead is counted from the command or
    // from the 9th byte; both are required here.
    speak_external_ = true;
    synth_offset_ = config_.se_shift;
  } else {
    return;
  }
  // DATASHEET GAP: §8.9 says both "at the start of that sample" and (step
  // 6) "a Speak latched in time starts here"; Z, U, loads and requested are
  // set at the start, talking at the end (step 6) (confirmed as intended).
  // UNMEASURED: the Speak External counter offset is kept until the next
  // Speak starts (§8.5 says only "for that utterance").
  zeroing_ = true;
  unvoiced_ = true;
  loads_ = 0;
  requested_ = true;
  utterance_frames_ = 0;
  new_frame_valid_ = false;
  SpeechEvent(
      speak_external_ ? Milestone::kStartExternal : Milestone::kStartSpeak,
      frame_pos_, 0);
}

void Tms5220::PeriodStart() {
  if (slot_ > 0) {
    frame_pos_ = (frame_pos_ + 1) % kSamplesPerFrame;
  }
  period_ = slot_ / kSlotsPerSample;

  CheckStart();
  synth_pos_ = (frame_pos_ + synth_offset_) % kSamplesPerFrame;
  const int p = synth_pos_;
  int ip = 0;
  int local = 0;
  if (p == 0) {
    ip = 7;
    local = 24;
  } else {
    ip = (p - 1) / 25;
    local = (p - 1) % 25;
  }
  const int pc_a = (local < 24 && (local & 1) == 0) ? local / 2 : -1;

  // oldU as it was in sample n-1 (step 2.4); remember it as it is now.
  bool excitation_unvoiced = old_unvoiced_previous_;
  old_unvoiced_previous_ = old_unvoiced_;

  // A frame is read in every frame-read sample while requested (§8.9).
  if (p == 0 && requested_ && !frame_active_) {
    FrameReadStart();
  }

  // 1. Queued writes due now.
  ApplyWrites();

  if (talking_) {
    // 2.1
    if (ip == 0 && local == 24 && new_frame_valid_) {
      LoadFrame();
    }
    // 2.2
    if (pc_a >= 0) {
      const auto at = static_cast<std::size_t>(pc_a);
      const bool now = inhibit_ && ip != 0;
      const int step =
          now ? 0
              : Asr(Target(pc_a) - current_[at],
                    kInterpolationShift[static_cast<std::size_t>(ip)]);
      bool zero = pc_a <= 5 ? zeroing_ : unvoiced_;
      int value = 0;
      if (pc_a >= 6 && now) {
        zero = false;
      }
      if (zero && zeroing_) {
        current_[at] += step;
        value = 0;
      } else if (zero) {
        current_[at] = 0;
        value = 0;
      } else {
        current_[at] += step;
        value = current_[at];
      }
      QueueWrite(period_ + kFilterDelay[at], pc_a, value);
    }
    // 2.3 In the A sample of PC 1 of IP 0 (local 2), clear hold: the
    // counter is held through PC 0 A and B and PC 1 A (§8.6).
    if (ip == 0 && local == 2) {
      hold_ = false;
    }
  } else {
    // 3.
    if (pc_a >= 0) {
      QueueWrite(period_ + kFilterDelay[static_cast<std::size_t>(pc_a)] - 1,
                 pc_a, 0);
    }
    excitation_unvoiced = old_unvoiced_;
  }

  // 2.4 Excitation (§8.6). Bit 0 of the noise register at slot 0.
  std::int32_t x = 0;
  if (excitation_unvoiced) {
    x = (noise_ & 1U) != 0 ? -kNoiseExcitation : kNoiseExcitation;
  } else {
    x = lpc::kChirpTable[static_cast<std::size_t>(
        std::min(count_, kLastChirp))];
  }

  // 4. Filter (§8.8).
  u_[11] = Wrap14(Booth(kExcitationScale * x, seen_[0]));
  for (std::size_t i = 10; i >= 1; --i) {
    u_[i] = Wrap14(u_[i + 1] - Booth(b_[i], seen_[1 + i]));
  }
  for (std::size_t i = 10; i >= 2; --i) {
    b_[i] = Wrap14(b_[i - 1] + Booth(u_[i - 1], seen_[i]));
  }
  b_[1] = u_[1];

  // 5. Output.
  code_ = Asr(u_[1], kCodeShift);
  if (sink_ != nullptr) {
    sink_->OnSample(period_, slot_, code_);
  }

  // 6. End of the period.
  if (p == 0) {
    if (inhibit_ && talking_) {
      hold_ = true;
    }
    old_silence_ = IsSilenceIndex(index_);
    old_unvoiced_ = IsUnvoicedIndex(index_);
    const bool was = talking_;
    talking_ = requested_;
    if (was && !talking_) {
      SpeechEvent(Milestone::kEnd, 0, 0);
    }
  }
  if (talking_) {
    count_ = count_ + 1;
    if (count_ >= seen_[1] + 1 || hold_) {
      count_ = 0;
    }
  }
}

// ---------------------------------------------------------------------
// Commands (§6)
// ---------------------------------------------------------------------

// A Read Byte latched (or, held, released) at slot `ref`, whose first VSM
// function may occur at slot `first_vsm`.
void Tms5220::ReadByteStart(int64_t ref, int64_t first_vsm) {
  int64_t first = 0;
  if (need_dummy_) {
    // §4: after a Load Address the Dummy Read comes first, 2 slots after
    // the latch; reading starts at the first slot 13 that is at least 22
    // slots after it.
    const int64_t m = first_vsm + kReadByteAfterDummy;
    Queue(first_vsm, Action::kDummy, 0);
    need_dummy_ = false;
    first = m + (kReadByteFirstSlot - m % kSlotsPerSample + kSlotsPerSample) %
                    kSlotsPerSample;
  } else {
    // §4: without a Dummy Read, slot 13 of the sample period after the one
    // in which the command was latched; held during speech, after the one
    // in which Talk Status cleared.
    first = (ref / kSlotsPerSample + 1) * kSlotsPerSample + kReadByteFirstSlot;
  }
  read_byte_done_ = false;
  read_byte_bits_ = 0;
  read_byte_value_ = 0;
  for (int i = 0; i < 8; ++i) {
    Queue(first + static_cast<int64_t>(i) * config_.vsm_spacing,
          Action::kReadByteBit, i);
  }
  // UNMEASURED (§3): a command written after Read Byte waits for the
  // eighth bit, then the normal latency.
  busy_until_ = first + 7 * static_cast<int64_t>(config_.vsm_spacing);
}

// Executes a command latched (or released from hold) at slot `ref`, whose
// first VSM function may occur at slot `first_vsm`.
void Tms5220::ExecuteCommand(int byte, int64_t ref, int64_t first_vsm) {
  const int command = (byte >> 4) & 7;
  switch (command) {
    case kCommandReadByte:
      ReadByteStart(ref, first_vsm);
      break;
    case kCommandReadAndBranch: {
      Queue(first_vsm, Action::kBranch, 0);
      need_dummy_ = false;  // §4: no Dummy Read needed after it
      busy_until_ = first_vsm;
      // §3 "When READY returns": READY for the next write goes active at
      // the start of the first slot 15 at least branch_ready slots after
      // the Branch function, whenever the write is made.
      int64_t ready = first_vsm + config_.branch_ready;
      ready += (kBranchReadySlot - ready % kSlotsPerSample + kSlotsPerSample) %
               kSlotsPerSample;
      ready_min_ = ready;
      break;
    }
    case kCommandLoadAddress:
      Queue(first_vsm, Action::kLoad, byte & 0xF);
      need_dummy_ = true;
      busy_until_ = first_vsm;
      break;
    default:
      break;
  }
}

void Tms5220::DoReset(int64_t latch) {
  const int64_t first_vsm = latch + config_.vsm_delay;
  // §6: Reset halts speech at once and resets the noise generator, not the
  // frame clock or the coded parameters. §8.9 "Not known for this path":
  // the output after a Reset during speech is treated as the end of speech
  // (not talking, step 3 from then on). DATASHEET GAP: what happens to Z,
  // U and the queued writes is not said; they are left as they are.
  if (talking_ || requested_ || talk_status_) {
    SpeechEvent(Milestone::kResetHalt, 0, 0);
  }
  queue_size_ = 0;  // cancel pending host-side actions
  talking_ = false;
  requested_ = false;
  frame_active_ = false;
  new_frame_valid_ = false;
  pending_speak_ = false;
  ts_clear_at_ = -1;
  external_active_ = false;
  external_ready_ = false;
  data_mode_ = false;
  // UNMEASURED (§3): what a held command does when a Reset arrives:
  // discarded.
  held_command_ = -1;
  SetFlag(StatusFlag::kTalkStatus, false);
  FifoPurge();
  // §7: Reset holds the noise register at 0x1FFF.
  noise_ = NoiseHoldState();
  noise_held_ = true;
  // §4: Load Address, then a Dummy Read.
  Queue(first_vsm, Action::kLoad, config_.reset_nibble);
  Queue(first_vsm + config_.vsm_spacing, Action::kDummy, 0);
  need_dummy_ = false;
  busy_until_ = first_vsm + config_.vsm_spacing;
  // UNMEASURED (§3): whether Reset releases INT (the original says so): it
  // does, after its own TS/BL changes.
  SetFlag(StatusFlag::kInterrupt, false);
}

// A write latched into the command register at slot `latch` (READY
// active).
void Tms5220::LatchCommand(int byte, int64_t latch) {
  const int command = (byte >> 4) & 7;
  const int64_t first_vsm = latch + config_.vsm_delay;

  data_mode_ = false;
  switch (command) {
    case kCommandReadByte:
    case kCommandReadAndBranch:
    case kCommandLoadAddress:
      if (command == kCommandReadByte) {
        data_mode_ = true;
      }
      if (talk_status_) {
        // §6: held until speech ends (TS clears).
        held_command_ = byte;
        SpeechEvent(Milestone::kHeld, byte, 0);
      } else {
        ExecuteCommand(byte, latch, first_vsm);
      }
      break;
    case kCommandSpeak:
      if (talk_status_ || pending_speak_) {
        // §6: ignored during speech. UNMEASURED (§3): a Speak while
        // another waits for its frame boundary: also ignored.
        SpeechEvent(Milestone::kIgnored, byte, 0);
        break;
      }
      if (need_dummy_) {
        // UNMEASURED (§3): when the Speak's Dummy Read occurs.
        Queue(first_vsm, Action::kDummy, 0);
        need_dummy_ = false;
      }
      busy_until_ = first_vsm;
      pending_speak_ = true;
      pending_latch_ = latch;
      Queue(latch + config_.ts_set_delay, Action::kTalkStatusSet, 0);
      if (noise_held_) {
        Queue(latch + config_.noise_rel_speak, Action::kNoiseRelease, 0);
      }
      break;
    case kCommandSpeakExternal:
      if (talk_status_ || pending_speak_) {
        // UNMEASURED (§3): Speak External during a VSM Speak: ignored.
        SpeechEvent(Milestone::kIgnored, byte, 0);
        break;
      }
      // Original §6.5: purge the FIFO (BL, BE set) and route writes to it.
      // UNMEASURED (§3): whether this purge raises INT: it does when it
      // sets BL (the §3 rule).
      FifoPurge();
      external_active_ = true;
      external_ready_ = false;
      external_latch_ = latch;
      if (noise_held_) {
        Queue(latch + config_.noise_rel_se, Action::kNoiseRelease, 0);
      }
      break;
    case kCommandReset:
      DoReset(latch);
      break;
    default:  // 0 and 2: NOP
      break;
  }
}

void Tms5220::TalkStatusCleared() {
  ts_clear_at_ = -1;
  SetFlag(StatusFlag::kTalkStatus, false);
  // UNMEASURED (§3): when writes stop going to the FIFO after a Speak
  // External ends: when TS clears.
  external_active_ = false;
  external_ready_ = false;
  if (held_command_ >= 0) {
    // §6: a held command reaches the VSM at the moment TS clears; a held
    // Read Byte reads from slot 13 of the next sample period (§4).
    const int byte = held_command_;
    held_command_ = -1;
    SpeechEvent(Milestone::kHeldExec, byte, 0);
    ExecuteCommand(byte, slot_, slot_);
  }
}

// ---------------------------------------------------------------------
// Host cycles
// ---------------------------------------------------------------------

int Tms5220::StatusByte() const {
  // §5: bit 7 TS, bit 6 BL, bit 5 BE, low five bits read as 1.
  return (talk_status_ ? 0x80 : 0) | (buffer_low_ ? 0x40 : 0) |
         (buffer_empty_ ? 0x20 : 0) | 0x1F;
}

void Tms5220::HostDone(int value) {
  host_active_ = false;
  host_value_ = value;
  if (observer_ != nullptr) {
    Tms5220Event event;
    event.type = EventType::kHost;
    event.slot = host_start_;
    event.host = host_kind_;
    event.value = value;
    event.start = host_start_;
    event.ready = slot_;
    Emit(event);
  }
}

void Tms5220::HostProgress() {
  const int64_t s = slot_;
  if (!host_active_) {
    return;
  }
  if (host_write_) {
    if (host_kind_ == HostCycle::kFifo) {
      if (s >= host_start_ + config_.fifo_latch) {
        FifoWrite(host_value_);
        HostDone(host_value_);
      }
      return;
    }
    if (held_command_ >= 0 && talk_status_) {
      // §3: a second command written while one is held keeps READY
      // inactive. UNMEASURED (§3): what releases it: here, the held
      // command running at the end of speech.
      return;
    }
    // UNMEASURED (§3): what a command waits for after Read Byte or a Load
    // Address: the latency counts from the end of the busy period. After
    // Read and Branch READY returns at ready_min (§3), or later with the
    // normal latency.
    const int64_t from = std::max(host_start_, busy_until_);
    const int64_t ready = std::max(from + config_.cmd_latch, ready_min_);
    if (s >= ready && held_command_ < 0) {
      LatchCommand(host_value_, s);
      HostDone(host_value_);
    }
    return;
  }
  // Read.
  if (s < host_start_ + config_.read_ready) {
    return;
  }
  if (data_mode_) {
    if (held_command_ >= 0) {
      // §3: while a Read Byte is held during speech, reads do not wait:
      // they return 0xFF at once and do not use it up. DATASHEET GAP:
      // whether such a read acts as a status read is not said; it is
      // logged as a data read and leaves INT as it is.
      host_kind_ = HostCycle::kData;
      HostDone(0xFF);
      return;
    }
    // §3: the data read's READY goes active 4 slots after the eighth bit
    // is read. DATASHEET GAP: for a read made later, the normal
    // status-read latency applies (whichever is later).
    if (!read_byte_done_ || s < read_byte_ready_at_) {
      return;
    }
    // UNMEASURED (§3): which reads return Read Byte data when it is not
    // held: only the first read after the Read Byte.
    data_mode_ = false;
    host_kind_ = HostCycle::kData;
    HostDone(read_byte_value_);
    return;
  }
  host_kind_ = HostCycle::kStatus;
  const int value = StatusByte();
  SetFlag(StatusFlag::kInterrupt, false);  // §3: a status read releases INT
  HostDone(value);
}

bool Tms5220::Write(int byte) {
  if (host_active_) {
    return false;
  }
  host_active_ = true;
  host_write_ = true;
  host_value_ = byte & 0xFF;
  host_start_ = slot_;
  // §5: during Speak External every write is FIFO data.
  host_kind_ = external_active_ ? HostCycle::kFifo : HostCycle::kCommand;
  return true;
}

bool Tms5220::Read() {
  if (host_active_) {
    return false;
  }
  host_active_ = true;
  host_write_ = false;
  host_start_ = slot_;
  host_kind_ = HostCycle::kStatus;
  return true;
}

bool Tms5220::IsTalking() const {
  return talk_status_ || talking_ || requested_ || pending_speak_ ||
         ts_clear_at_ >= 0;
}

int64_t Tms5220::NextBoundary() const {
  // The next slot at which a VSM frame read would start (frame_pos 0, slot
  // frame_read_slot), at or after the current slot.
  const int64_t period = slot_ / kSlotsPerSample;
  const auto slot_in_period = static_cast<int>(slot_ % kSlotsPerSample);
  int pos = frame_pos_;
  if (slot_in_period == 0 && slot_ != 0) {
    pos = (pos + 1) % kSamplesPerFrame;  // period not started yet
  }
  int64_t boundary =
      (period + (kSamplesPerFrame - pos) % kSamplesPerFrame) * kSlotsPerSample +
      config_.frame_read_slot;
  if (boundary < slot_) {
    boundary += kSlotsPerFrame;
  }
  return boundary;
}

// ---------------------------------------------------------------------
// The slot
// ---------------------------------------------------------------------

void Tms5220::Step() {
  if (slot_ % kSlotsPerSample == 0) {
    PeriodStart();
  }
  if (ts_clear_at_ >= 0 && slot_ >= ts_clear_at_) {
    TalkStatusCleared();
  }
  RunQueue();
  if (frame_active_ && slot_ == frame_next_) {
    FrameReadSlot();
  }
  HostProgress();
  // §7, §8.6: the noise register steps once per slot unless held.
  if (!noise_held_) {
    noise_ = NoiseStep(noise_);
  }
  ++slot_;
}

}  // namespace speak5220::core::emulator
