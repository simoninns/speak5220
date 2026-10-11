/*
 * File:        tms5220_test.cpp
 * Module:      emulator tests
 * Purpose:     Unit tests of the TMS5220 emulator against the datasheet,
 *              with the VSM bus, sample sink and observer mocked
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/tms5220.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "core/emulator/tms5220_config.h"
#include "core/emulator/tms5220_event.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/emulator_mocks.h"
#include "support/frame_builder.h"

namespace speak5220::core::emulator {
namespace {

using speak5220::testing::BitStream;
using speak5220::testing::kNeutralUnvoicedK;
using speak5220::testing::kNeutralVoicedK;
using speak5220::testing::MockSampleSink;
using speak5220::testing::MockTms5220Observer;
using speak5220::testing::MockVsmBus;
using ::testing::_;
using ::testing::InSequence;
using ::testing::NiceMock;

// The chip with mocked collaborators. The VSM mock returns the bits it is
// given to serve, in order, then zeros; the observer and sink mocks record
// what the tests check. Re-implements the harness of test_tms5220.c.
class ChipHarness {
 public:
  explicit ChipHarness(const Tms5220Config& config = {})
      : chip(config, &vsm, &sink, &observer) {
    ON_CALL(vsm, Read()).WillByDefault([this] { return NextBit(); });
    ON_CALL(observer, OnEvent(_)).WillByDefault([this](const Tms5220Event& e) {
      Record(e);
    });
    ON_CALL(sink, OnSample(_, _, _))
        .WillByDefault([this](std::int64_t index, std::int64_t, int code) {
          RecordSample(index, code);
        });
  }

  // The VSM returns these bits for the following Read functions.
  void ServeBits(const std::vector<int>& bits) {
    bits_ = bits;
    next_bit_ = 0;
  }
  void ServeBytes(std::span<const std::uint8_t> bytes) {
    std::vector<int> bits;
    for (const std::uint8_t byte : bytes) {
      for (int i = 0; i < 8; ++i) {
        bits.push_back((byte >> i) & 1);  // §6: LSB first
      }
    }
    ServeBits(bits);
  }

  void Run(int slots) {
    for (int i = 0; i < slots; ++i) {
      chip.Step();
    }
  }
  void Write(int byte) {
    ASSERT_TRUE(chip.Write(byte));
    while (chip.HostBusy()) {
      chip.Step();
    }
  }
  int Read() {
    EXPECT_TRUE(chip.Read());
    while (chip.HostBusy()) {
      chip.Step();
    }
    return chip.HostValue();
  }
  void LoadAddress(std::uint32_t address) {
    for (int i = 0; i < 5; ++i) {
      Write(0x40 | static_cast<int>((address >> (4 * i)) & 0xF));
    }
  }
  void RunWhileTalking() {
    while (chip.IsTalking()) {
      chip.Step();
    }
  }
  void ResetCounts() {
    reads.clear();
    read_bits.clear();
    dummies = 0;
    loads = 0;
    non_idle = 0;
    min_code = 1 << 20;
    max_code = -(1 << 20);
  }

  NiceMock<MockVsmBus> vsm;
  NiceMock<MockSampleSink> sink;
  NiceMock<MockTms5220Observer> observer;
  Tms5220 chip;

  // Recorded events.
  std::vector<std::int64_t> reads;  // slots of VSM Read functions
  std::vector<int> read_bits;       // the bits they returned
  int dummies = 0;
  int loads = 0;
  std::int64_t dummy_slot = -1;
  std::int64_t branch_slot = -1;
  HostCycle host_kind = HostCycle::kStatus;
  std::int64_t host_start = -1;
  std::int64_t host_ready = -1;
  std::int64_t ts_set_slot = -1;
  std::int64_t ts_clear_slot = -1;
  // Recorded samples.
  std::int64_t samples = 0;
  int min_code = 1 << 20;
  int max_code = -(1 << 20);
  int non_idle = 0;
  std::int64_t first_non_idle = -1;

 private:
  int NextBit() { return next_bit_ < bits_.size() ? bits_[next_bit_++] : 0; }

  void Record(const Tms5220Event& e) {
    if (e.type == EventType::kVsm) {
      if (e.function == VsmFunction::kRead) {
        reads.push_back(e.slot);
        read_bits.push_back(e.argument);
      } else if (e.function == VsmFunction::kDummy) {
        ++dummies;
        dummy_slot = e.slot;
      } else if (e.function == VsmFunction::kBranch) {
        branch_slot = e.slot;
      } else {
        ++loads;
      }
    } else if (e.type == EventType::kHost) {
      host_kind = e.host;
      host_start = e.start;
      host_ready = e.ready;
    } else if (e.type == EventType::kFlag &&
               e.flag == StatusFlag::kTalkStatus) {
      (e.state ? ts_set_slot : ts_clear_slot) = e.slot;
    }
  }

  void RecordSample(std::int64_t index, int code) {
    ++samples;
    min_code = std::min(min_code, code);
    max_code = std::max(max_code, code);
    if (code != -1) {
      if (non_idle == 0) {
        first_non_idle = index;
      }
      ++non_idle;
    }
  }

  std::vector<int> bits_;
  std::size_t next_bit_ = 0;
};

// §4 "When Read Byte reads": slot 13 of the sample period after `ref` (no
// Dummy Read), or the first slot 13 at least 22 slots after the Dummy Read.
std::int64_t ReadByteAfterPeriod(std::int64_t ref) {
  return (ref / 20 + 1) * 20 + 13;
}
std::int64_t ReadByteAfterDummy(std::int64_t dummy) {
  const std::int64_t m = dummy + 22;
  return m + (13 - m % 20 + 20) % 20;
}

// Eight bits two slots apart from `want`, in slots 13, 15, ..., 7; §3: the
// data read's READY 4 slots after the eighth bit.
void ExpectReadByteTiming(const ChipHarness& h, std::int64_t want) {
  constexpr std::array<int, 8> kSlots = {13, 15, 17, 19, 1, 3, 5, 7};
  ASSERT_EQ(h.reads.size(), 8U);
  EXPECT_EQ(h.reads[0], want);
  for (std::size_t i = 0; i < 8; ++i) {
    EXPECT_EQ(h.reads[i] % 20, kSlots.at(i)) << "bit " << i;
    EXPECT_EQ(h.reads[i] - h.reads[0], static_cast<std::int64_t>(2 * i));
  }
  EXPECT_EQ(h.host_kind, HostCycle::kData);
  EXPECT_EQ(h.host_ready, h.reads[7] + 4);
}

// Re-implements test_tms5220.c test_idle.
TEST(Tms5220Test, IdleOutputIsMinusOne) {
  ChipHarness h;
  h.Run(20 * 4000);  // 4000 samples, 20 frames
  EXPECT_EQ(h.samples, 4000);
  EXPECT_EQ(h.non_idle, 0);
  EXPECT_EQ(h.min_code, -1);
  EXPECT_EQ(h.max_code, -1);
  EXPECT_EQ(h.chip.LatticeU1(), -9);  // §8.8
}

TEST(Tms5220Test, SinkReceivesEverySamplePeriodAtSlotZero) {
  ChipHarness h;
  {
    InSequence order;
    EXPECT_CALL(h.sink, OnSample(0, 0, -1));
    EXPECT_CALL(h.sink, OnSample(1, 20, -1));
    EXPECT_CALL(h.sink, OnSample(2, 40, -1));
  }
  h.Run(60);
}

TEST(Tms5220Test, StatusAfterPowerUpAndResetIs7F) {
  ChipHarness h;
  h.Run(1000);
  EXPECT_EQ(h.Read(), 0x7F);
  {
    // §4: Reset issues Load Address (nibble 8), then a Dummy Read.
    InSequence order;
    EXPECT_CALL(h.vsm, Load(8));
    EXPECT_CALL(h.vsm, Dummy());
  }
  h.Write(0x70);
  EXPECT_EQ(h.Read(), 0x7F);
  EXPECT_EQ(h.loads, 1);
  EXPECT_EQ(h.dummies, 1);
}

// Re-implements test_tms5220.c test_frame_lengths.
TEST(Tms5220Test, ReadsEachFrameKindAtParameterOffsets) {
  // §4 "The rest of the frame": the first bit of each parameter, in slots
  // from the frame's first bit, and its width (repeat and pitch as one).
  constexpr std::array<int, 12> kParameterSlot = {0,   34,  78,  118, 160, 200,
                                                  240, 280, 320, 362, 402, 442};
  constexpr std::array<int, 12> kParameterBits = {4, 7, 5, 5, 4, 4,
                                                  4, 4, 4, 3, 3, 3};
  constexpr std::array<std::size_t, 5> kFrameBits = {50, 11, 29, 4, 4};
  std::vector<int> offsets;
  for (std::size_t p = 0; p < kParameterSlot.size(); ++p) {
    for (int q = 0; q < kParameterBits.at(p); ++q) {
      offsets.push_back(kParameterSlot.at(p) + 2 * q);
    }
  }
  BitStream stream;
  stream.Voiced(10, 30, kNeutralVoicedK);
  stream.Repeat(10, 30);
  stream.Unvoiced(8, kNeutralUnvoicedK);
  stream.Silence();
  stream.Stop();
  stream.Put(0xFF, 8);  // data after the stop code: must not be read

  for (int phase = 0; phase < 200; phase += 37) {
    SCOPED_TRACE(::testing::Message() << "phase " << phase);
    ChipHarness h;
    h.chip.SetPhase(phase);
    h.ServeBits(stream.Bits());
    h.Run(1000);
    {
      // The address 0x01234, low nibble first.
      InSequence order;
      for (const int nibble : {4, 3, 2, 1, 0}) {
        EXPECT_CALL(h.vsm, Load(nibble));
      }
    }
    h.LoadAddress(0x1234);
    h.ResetCounts();
    h.Write(0x50);
    h.RunWhileTalking();
    h.Run(4000);

    EXPECT_EQ(h.dummies, 1) << "one Dummy Read before the first frame";
    // Group the reads into frames by the gap between them.
    std::vector<std::size_t> first;
    std::vector<std::size_t> length;
    for (std::size_t i = 0; i < h.reads.size(); ++i) {
      if (i == 0 || h.reads[i] - h.reads[i - 1] > 1000) {
        first.push_back(i);
        length.push_back(0);
      }
      ++length.back();
    }
    ASSERT_EQ(first.size(), 5U);
    for (std::size_t j = 0; j < first.size(); ++j) {
      EXPECT_EQ(length[j], kFrameBits.at(j)) << "frame " << j;
      for (std::size_t i = 0; i < length[j]; ++i) {
        EXPECT_EQ(h.reads[first[j] + i] - h.reads[first[j]], offsets[i])
            << "frame " << j << " bit " << i;
      }
      // Frames 25 ms (4000 slots) apart.
      EXPECT_EQ((h.reads[first[j]] - h.reads[0]) % 4000, 0);
    }
    EXPECT_EQ(h.reads[first[0] + 49] - h.reads[0], 446);
    EXPECT_EQ(h.reads[0] % 20, 17) << "first bit in slot 17";
    // §4: TS clears 24.87 ms after the stop frame is read.
    const std::int64_t stop_read = h.reads[h.reads.size() - 4];
    EXPECT_EQ(h.ts_clear_slot - stop_read, 3979);
    EXPECT_GT(h.non_idle, 0);
    // §7: the first sample that differs from idle 206-207 samples after
    // the first frame read.
    EXPECT_GE(h.first_non_idle - h.reads[0] / 20, 205);
    EXPECT_LE(h.first_non_idle - h.reads[0] / 20, 208);
  }
}

// Re-implements test_tms5220.c test_stop_first.
TEST(Tms5220Test, StopCodeAsFirstFrameClearsTalkStatusAfter14Slots) {
  ChipHarness h;
  h.chip.SetPhase(77);
  const std::array<std::uint8_t, 2> image = {0x0F, 0xFF};
  h.ServeBytes(image);
  h.Run(300);
  h.LoadAddress(0x200);
  h.ResetCounts();
  h.Write(0x50);
  h.RunWhileTalking();
  h.Run(20 * 1000);
  EXPECT_EQ(h.reads.size(), 4U) << "a first-frame stop reads 4 bits";
  EXPECT_EQ(h.non_idle, 0) << "and gives no output change";
  ASSERT_FALSE(h.reads.empty());
  EXPECT_EQ(h.ts_clear_slot - h.reads[0], 14);  // 0.09 ms
  EXPECT_TRUE(h.chip.InterruptActive()) << "INT active after TS clears";
  h.Read();
  EXPECT_FALSE(h.chip.InterruptActive()) << "a status read releases INT";
}

// Re-implements test_tms5220.c test_speak_external.
TEST(Tms5220Test, SpeakExternalStartsWithNinthByte) {
  BitStream stream;
  for (int i = 0; i < 3; ++i) {
    stream.Voiced(10, 30, kNeutralVoicedK);  // 150 bits
  }
  stream.Stop();  // 154 bits: 20 bytes
  const std::vector<std::uint8_t> bytes = stream.Bytes();
  ASSERT_EQ(bytes.size(), 20U);
  ChipHarness h;
  // Speak External reads only the FIFO.
  EXPECT_CALL(h.vsm, Read()).Times(0);
  EXPECT_CALL(h.vsm, Load(_)).Times(0);
  h.Run(100);
  h.Write(0x60);
  EXPECT_EQ(h.Read(), 0x7F) << "after Speak External: BL, BE set";
  for (std::size_t i = 0; i < 8; ++i) {
    h.Write(bytes[i]);
  }
  EXPECT_EQ(h.Read(), 0x5F) << "8 bytes: BE clear";
  h.Run(20 * 1000);
  EXPECT_EQ(h.non_idle, 0);
  EXPECT_EQ(h.Read() & 0x80, 0) << "8 bytes or fewer: speech never starts";
  h.Write(bytes[8]);
  EXPECT_EQ(h.Read(), 0x9F) << "9th byte: TS set, BL clear";
  for (std::size_t i = 9; i < 16; ++i) {
    h.Write(bytes[i]);
  }
  while ((h.Read() & 0x40) == 0) {
    h.Run(160);
  }
  for (std::size_t i = 16; i < 20; ++i) {
    h.Write(bytes[i]);
  }
  while ((h.Read() & 0x80) != 0) {
    h.Run(160);
  }
  EXPECT_GT(h.non_idle, 0) << "Speak External speaks";
}

// Re-implements test_tms5220.c test_read_byte.
TEST(Tms5220Test, ReadByteReturnsBitReversedBytesAtSlot13) {
  const std::array<std::uint8_t, 4> image = {0x23, 0x9B, 0x35, 0x85};
  bool seen21 = false;
  bool seen22 = false;
  bool seen14 = false;
  for (int k = 0; k < 20; ++k) {
    SCOPED_TRACE(::testing::Message() << "start slot " << k);
    ChipHarness h;
    h.ServeBytes(image);
    h.Run(k);
    h.LoadAddress(0x3C140);
    h.ResetCounts();
    h.Write(0x10);
    EXPECT_EQ(h.Read(), 0xC4);  // §6: 0x23 read as 0xC4
    EXPECT_EQ(h.dummies, 1) << "Dummy Read before the first Read Byte";
    const std::int64_t dummy = h.dummy_slot;
    ExpectReadByteTiming(h, ReadByteAfterDummy(dummy));
    // §4's measured cases: a slot 13 21 or 14 slots after the Dummy Read
    // is not used, one 22 slots after is.
    if (dummy % 20 == 12) {
      seen21 = true;
      EXPECT_EQ(h.reads[0] - dummy, 41);
    }
    if (dummy % 20 == 11) {
      seen22 = true;
      EXPECT_EQ(h.reads[0] - dummy, 22);
    }
    if (dummy % 20 == 19) {
      seen14 = true;
      EXPECT_EQ(h.reads[0] - dummy, 34);
    }
    h.ResetCounts();
    h.Write(0x10);
    const std::int64_t latch = h.host_ready;
    EXPECT_EQ(h.Read(), 0xD9);
    ExpectReadByteTiming(h, ReadByteAfterPeriod(latch));
    h.Write(0x10);
    EXPECT_EQ(h.Read(), 0xAC);
    h.Write(0x10);
    EXPECT_EQ(h.Read(), 0xA1);
    EXPECT_EQ(h.dummies, 0) << "one Dummy Read for four Read Bytes";
  }
  EXPECT_TRUE(seen21 && seen22 && seen14);
}

// Re-implements test_tms5220.c test_read_byte_held: §3, §4, §6.
TEST(Tms5220Test, ReadByteHeldDuringSpeechReturnsFFThenItsByte) {
  BitStream stream;
  stream.Voiced(10, 30, kNeutralVoicedK);
  stream.Voiced(10, 30, kNeutralVoicedK);
  stream.Stop();
  stream.Put(0x5A, 8);
  ChipHarness h;
  h.chip.SetPhase(120);
  h.ServeBits(stream.Bits());
  h.Run(500);
  h.LoadAddress(0x800);
  h.Write(0x50);
  h.Run(400);
  EXPECT_NE(h.Read() & 0x80, 0) << "talking";
  h.Write(0x10);
  for (int i = 0; i < 2; ++i) {
    EXPECT_EQ(h.Read(), 0xFF) << "read " << i << " while held";
    EXPECT_EQ(h.host_ready - h.host_start, h.chip.Config().read_ready)
        << "a read while held does not wait";
  }
  h.ts_clear_slot = -1;
  while (h.ts_clear_slot < 0) {
    h.chip.Step();
  }
  h.ResetCounts();
  const int value = h.Read();
  int want = 0;
  for (std::size_t i = 0; i < 8 && i < h.read_bits.size(); ++i) {
    want |= h.read_bits[i] << (7 - i);
  }
  EXPECT_EQ(value, want);
  ExpectReadByteTiming(h, ReadByteAfterPeriod(h.ts_clear_slot));
}

// Re-implements test_tms5220.c test_branch_ready: §3 "When READY returns".
TEST(Tms5220Test, ReadyAfterReadAndBranchComesAtSlot15) {
  bool seen12 = false;
  bool seen16 = false;
  const std::array<std::uint8_t, 1> target = {0x23};
  for (int n = 60; n <= 63; ++n) {
    for (int k = 0; k < 20; ++k) {
      SCOPED_TRACE(::testing::Message() << "limit " << n << " start " << k);
      Tms5220Config config;
      config.branch_ready = n;
      ChipHarness h(config);
      // After the Branch the VSM reads from the target.
      ON_CALL(h.vsm, Branch()).WillByDefault([&h, &target] {
        h.ServeBytes(target);
      });
      h.Run(k);
      h.LoadAddress(0x3C140);
      h.Write(0x30);
      if ((k & 1) != 0) {
        h.Run(30);  // a later write: the same READY
      }
      h.ResetCounts();
      h.Write(0x10);
      const std::int64_t d = h.host_ready - h.branch_slot;
      EXPECT_EQ(h.host_ready % 20, 15);
      EXPECT_TRUE(d >= n && d - 20 < n) << d;
      EXPECT_TRUE(d > 59 && d - 20 < 63) << d;
      if (h.branch_slot % 20 == 16) {
        seen16 = true;
        EXPECT_EQ(d, 79);
      }
      if (h.branch_slot % 20 == 12) {
        seen12 = true;
        EXPECT_EQ(d, 63);
      }
      // §4: a Read Byte straight after it reads the target, with no Dummy
      // Read; it is latched when READY returns.
      const std::int64_t latch = h.host_ready;
      EXPECT_EQ(h.Read(), 0xC4);
      EXPECT_EQ(h.dummies, 0);
      ExpectReadByteTiming(h, ReadByteAfterPeriod(latch));
    }
  }
  EXPECT_TRUE(seen12 && seen16);
}

TEST(Tms5220Test, HostCycleInProgressRefusesAnother) {
  ChipHarness h;
  ASSERT_TRUE(h.chip.Write(0x70));
  EXPECT_FALSE(h.chip.Write(0x70));
  EXPECT_FALSE(h.chip.Read());
  h.chip.AbandonHostCycle();
  EXPECT_TRUE(h.chip.Read());
}

TEST(Tms5220Test, NextBoundaryIsFrameReadSlot) {
  ChipHarness h;
  EXPECT_EQ(h.chip.NextBoundary(), 17);
  h.Run(18);
  EXPECT_EQ(h.chip.NextBoundary(), 17 + Tms5220::kSlotsPerFrame);
}

}  // namespace
}  // namespace speak5220::core::emulator
