/*
 * File:        emulator_run.cpp
 * Module:      test support
 * Purpose:     Runs the emulator as the retired tms5220emu program did,
 *              producing the same codes and event log
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/emulator_run.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <iomanip>
#include <memory>
#include <optional>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/emulator/chip_driver.h"
#include "core/emulator/sample_sink.h"
#include "core/emulator/tms5220.h"
#include "core/emulator/tms5220_config.h"
#include "core/emulator/tms6100.h"
#include "core/lpc/coding_tables.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

using core::InvalidArgumentError;
using core::OkStatus;
using core::Status;
using core::StatusOr;
using core::emulator::ChipDriver;
using core::emulator::EventType;
using core::emulator::HostCycle;
using core::emulator::StatusFlag;
using core::emulator::Tms5220;
using core::emulator::Tms5220Config;
using core::emulator::Tms5220Event;
using core::emulator::Tms6100;
using core::emulator::VsmFunction;

std::string_view HostName(HostCycle host) {
  switch (host) {
    case HostCycle::kCommand:
      return "WRITE_CMD";
    case HostCycle::kFifo:
      return "WRITE_FIFO";
    case HostCycle::kStatus:
      return "READ_STATUS";
    case HostCycle::kData:
      return "READ_DATA";
  }
  return "?";
}

std::string_view FlagName(StatusFlag flag) {
  switch (flag) {
    case StatusFlag::kTalkStatus:
      return "TS";
    case StatusFlag::kBufferLow:
      return "BL";
    case StatusFlag::kBufferEmpty:
      return "BE";
    case StatusFlag::kInterrupt:
      return "INT";
  }
  return "?";
}

std::optional<std::int64_t> ParseNumber(const std::string& text, int base) {
  std::istringstream stream(text);
  std::int64_t value = 0;
  if (!(stream >> std::setbase(base) >> value) || !stream.eof()) {
    return std::nullopt;
  }
  return value;
}

// The bytes from the start of `memory` up to and including the last bit of
// the first stop frame (§8.1); nothing if there is no stop code before the
// end of `memory`. Re-implements tms5220emu.c phrase_bytes().
std::optional<std::size_t> PhraseBytes(std::span<const std::uint8_t> memory) {
  const std::uint64_t max_bits = std::uint64_t{memory.size()} * 8;
  std::uint64_t bit = 0;
  auto read = [&](int width) -> std::optional<int> {
    int value = 0;
    for (int i = 0; i < width; ++i) {
      if (bit >= max_bits) {
        return std::nullopt;
      }
      const int b = (memory[bit / 8] >> (bit % 8)) & 1;
      value = (value << 1) | b;
      ++bit;
    }
    return value;
  };
  while (true) {
    const std::optional<int> energy = read(4);
    if (!energy) {
      return std::nullopt;
    }
    if (*energy == 15) {
      return static_cast<std::size_t>((bit + 7) / 8);
    }
    if (*energy == 0) {
      continue;
    }
    const std::optional<int> repeat = read(1);
    const std::optional<int> pitch = read(6);
    if (!repeat || !pitch) {
      return std::nullopt;
    }
    if (*repeat != 0) {
      continue;
    }
    const std::size_t count = *pitch != 0 ? 10 : 4;
    for (std::size_t j = 0; j < count; ++j) {
      if (!read(core::lpc::kKBits.at(j))) {
        return std::nullopt;
      }
    }
  }
}

// One emulator with its VSM, recording codes and events. The driver is
// created when the first operation needs the chip, so `phase` and `set`
// can come first, as tms5220emu allows before anything runs.
class Runner : public core::emulator::ISampleSink,
               public core::emulator::ITms5220Observer {
 public:
  void OnSample(std::int64_t /*index*/, std::int64_t /*slot*/,
                int code) override {
    run_.codes.push_back(code);
  }
  void OnEvent(const Tms5220Event& event) override {
    run_.events.push_back(FormatEvent(event));
  }

  Tms6100& Vsm() { return vsm_; }

  Status SetPhase(int phase) {
    if (driver_ && driver_->Chip().Slot() != 0) {
      return InvalidArgumentError("phase must be set before anything runs");
    }
    phase_ = phase;
    driver_.reset();
    return OkStatus();
  }

  Status SetParameter(std::string_view name, int value) {
    if (driver_ && driver_->Chip().Slot() != 0) {
      return InvalidArgumentError("set must be used before anything runs");
    }
    driver_.reset();
    return core::emulator::SetConfigParameter(config_, name, value);
  }

  ChipDriver& Driver() {
    if (!driver_) {
      driver_ = std::make_unique<ChipDriver>(config_, &vsm_, this, this);
      driver_->Chip().SetPhase(phase_);
      driver_->SetMarkHandler([this](std::int64_t slot, std::string_view name) {
        run_.events.push_back(std::to_string(slot) + " MARK " +
                              std::string(name));
      });
    }
    return *driver_;
  }

  EmulatorRun TakeRun() { return std::move(run_); }

 private:
  Tms5220Config config_;
  int phase_ = 0;
  Tms6100 vsm_;
  std::unique_ptr<ChipDriver> driver_;
  EmulatorRun run_;
};

// Runs one script line. Re-implements tms5220emu.c run_line().
Status RunLine(Runner& runner, std::string line,
               const std::filesystem::path& directory) {
  if (const std::size_t hash = line.find('#'); hash != std::string::npos) {
    line.resize(hash);
  }
  std::istringstream stream(line);
  std::vector<std::string> argv;
  for (std::string token; stream >> token;) {
    argv.push_back(token);
  }
  if (argv.empty()) {
    return OkStatus();
  }
  const std::string& op = argv[0];
  auto number = [&](std::size_t i, int base) -> StatusOr<std::int64_t> {
    if (i >= argv.size()) {
      return InvalidArgumentError("missing argument to " + op);
    }
    const std::optional<std::int64_t> value = ParseNumber(argv[i], base);
    if (!value) {
      return InvalidArgumentError("bad number " + argv[i]);
    }
    return *value;
  };
  if (op == "load") {
    StatusOr<std::int64_t> address = number(2, 16);
    if (!address.IsOk()) {
      return address.GetStatus();
    }
    StatusOr<std::vector<std::uint8_t>> data = ReadBytes(directory / argv[1]);
    if (!data.IsOk()) {
      return data.GetStatus();
    }
    runner.Vsm().LoadMemory(*data, static_cast<std::uint32_t>(*address));
    return OkStatus();
  }
  if (op == "phase") {
    StatusOr<std::int64_t> phase = number(1, 10);
    return phase.IsOk() ? runner.SetPhase(static_cast<int>(*phase))
                        : phase.GetStatus();
  }
  if (op == "set") {
    StatusOr<std::int64_t> value = number(2, 10);
    return value.IsOk() ? runner.SetParameter(argv[1], static_cast<int>(*value))
                        : value.GetStatus();
  }
  ChipDriver& driver = runner.Driver();
  if (op == "slot" || op == "at" || op == "wait" || op == "until_idle" ||
      op == "boundary") {
    const bool optional = op == "until_idle" && argv.size() < 2;
    StatusOr<std::int64_t> value =
        optional ? StatusOr<std::int64_t>(Tms5220::kSlotsPerFrame)
                 : number(1, 10);
    if (!value.IsOk()) {
      return value.GetStatus();
    }
    const std::int64_t now = driver.Chip().Slot();
    if (op == "slot") {
      if (*value < now) {
        return InvalidArgumentError("slot is in the past: " + argv[1]);
      }
      driver.StepTo(*value);
    } else if (op == "at") {
      driver.StepTo(*value);
    } else if (op == "wait") {
      driver.StepTo(now + *value);
    } else if (op == "until_idle") {
      driver.UntilIdle(*value);
    } else {
      driver.Boundary(*value);
    }
    return OkStatus();
  }
  if (op == "write" || op == "writes") {
    const std::size_t last = op == "write" ? 2 : argv.size();
    for (std::size_t i = 1; i < last; ++i) {
      StatusOr<std::int64_t> byte = number(i, 16);
      if (!byte.IsOk()) {
        return byte.GetStatus();
      }
      if (Status status = driver.Write(static_cast<int>(*byte));
          !status.IsOk()) {
        return status;
      }
    }
    return OkStatus();
  }
  if (op == "status" || op == "read") {
    return driver.Read().GetStatus();
  }
  if (op == "mark") {
    driver.Mark(argv.size() > 1 ? argv[1] : "");
    return OkStatus();
  }
  return InvalidArgumentError("unknown operation " + op);
}

// Runs every line of `script`, `load` paths relative to its directory.
// Re-implements tms5220emu.c run_script().
Status RunScript(Runner& runner, const std::filesystem::path& script) {
  StatusOr<std::vector<std::string>> lines = ReadLines(script);
  if (!lines.IsOk()) {
    return lines.GetStatus();
  }
  for (std::size_t n = 0; n < lines->size(); ++n) {
    Status status = RunLine(runner, (*lines)[n], script.parent_path());
    if (!status.IsOk()) {
      return InvalidArgumentError(script.string() + ":" +
                                  std::to_string(n + 1) + ": " +
                                  status.Message());
    }
  }
  return OkStatus();
}

}  // namespace

std::string FormatEvent(const Tms5220Event& event) {
  std::string line;
  switch (event.type) {
    case EventType::kVsm:
      line = std::to_string(event.slot) + " VSM ";
      switch (event.function) {
        case VsmFunction::kLoad:
          return line + "LOAD " + std::to_string(event.argument);
        case VsmFunction::kRead:
          return line + "READ " + std::to_string(event.argument);
        case VsmFunction::kDummy:
          return line + "DUMMY";
        case VsmFunction::kBranch:
          return line + "BRANCH";
      }
      return line;
    case EventType::kHost: {
      constexpr std::size_t kHexSize = 8;
      std::array<char, kHexSize> hex = {};
      std::snprintf(hex.data(), hex.size(), "0x%02X", event.value);
      return std::to_string(event.start) + " HOST " +
             std::string(HostName(event.host)) + " " + hex.data() + " ready " +
             std::to_string(event.ready);
    }
    case EventType::kFlag:
      return std::to_string(event.slot) + " FLAG " +
             std::string(FlagName(event.flag)) + " " +
             (event.state ? "1" : "0");
    case EventType::kSpeech:
      return std::to_string(event.slot) + " INFO " +
             std::string(core::emulator::MilestoneName(event.milestone)) + " " +
             std::to_string(event.a) + " " + std::to_string(event.b);
  }
  return line;
}

StatusOr<EmulatorRun> RunScenarioScript(const std::filesystem::path& script) {
  Runner runner;
  if (Status status = RunScript(runner, script); !status.IsOk()) {
    return status;
  }
  return runner.TakeRun();
}

StatusOr<EmulatorRun> RunBulk(const BulkRun& run) {
  StatusOr<std::vector<std::uint8_t>> data = ReadBytes(run.image);
  if (!data.IsOk()) {
    return data.GetStatus();
  }
  Runner runner;
  if (Status status = runner.SetPhase(run.phase); !status.IsOk()) {
    return status;
  }
  runner.Vsm().LoadMemory(*data, run.load_at);
  if (run.prior) {
    if (Status status = RunScript(runner, *run.prior); !status.IsOk()) {
      return status;
    }
  }
  ChipDriver& driver = runner.Driver();
  Status status;
  if (run.external) {
    // The VSM's memory as loaded: the stream is read from it.
    std::vector<std::uint8_t> memory(Tms6100::kMemorySize, 0);
    for (std::size_t i = 0; i < data->size(); ++i) {
      memory[(run.load_at + i) % memory.size()] = (*data)[i];
    }
    const auto address =
        static_cast<std::uint32_t>(run.address % memory.size());
    // tms5220emu sends 256 bytes when it finds no stop code.
    constexpr std::size_t kNoStopBytes = 256;
    const std::size_t length =
        PhraseBytes(std::span(memory).subspan(address)).value_or(kNoStopBytes);
    std::vector<std::uint8_t> stream(length);
    for (std::size_t i = 0; i < length; ++i) {
      stream[i] = memory[(address + i) % memory.size()];
    }
    status = driver.SpeakExternal(stream, run.speak);
  } else {
    status = driver.SpeakFromVsm(run.address, run.speak);
  }
  if (!status.IsOk()) {
    return status;
  }
  return runner.TakeRun();
}

StatusOr<EmulatorRun> RunBulk(const std::filesystem::path& image, bool external,
                              int phase) {
  BulkRun run;
  run.image = image;
  run.phase = phase;
  run.external = external;
  return RunBulk(run);
}

}  // namespace speak5220::testing
