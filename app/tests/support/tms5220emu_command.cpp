/*
 * File:        tms5220emu_command.cpp
 * Module:      test support
 * Purpose:     The tms5220emu command line on the application's emulator,
 *              for the reference-data and research scripts
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/tms5220emu_command.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ostream>
#include <span>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "core/emulator/tms5220.h"
#include "core/util/status.h"
#include "support/emulator_run.h"

namespace {

namespace emulator = speak5220::core::emulator;

// Exit code for a command line or run that cannot be used, as tms5220emu's.
constexpr int kExitError = 2;

int Usage() {
  std::cerr
      << "usage: tms5220emu [-s SAMPLES] [-e EVENTS] [-q] SCRIPT\n"
         "       tms5220emu [-s SAMPLES] [-e EVENTS] [-q] --image FILE --addr "
         "HEX\n"
         "                  [--load-at HEX] [--speak-at SLOTS] [--external] "
         "[--reset]\n"
         "                  [--prior SCRIPT] [--phase SAMPLE] [--margin "
         "SLOTS]\n"
         "  -s FILE   write the I/O code of every sample period (default "
         "stdout)\n"
         "  -e FILE   write the event log (default stdout; '-' = stdout, "
         "'none' = off)\n"
         "  -q        omit VSM READ functions from the event log\n"
         "Bulk mode: load FILE at --load-at (default: --addr), run --prior, "
         "then speak\n"
         "the phrase at --addr with the Speak (or Speak External) command "
         "accepted\n"
         "--speak-at slots (default 1961) before a frame boundary.\n";
  return kExitError;
}

int Fail(std::string_view message) {
  std::cerr << "tms5220emu: " << message << "\n";
  return kExitError;
}

std::optional<std::int64_t> ParseNumber(const std::string& text, int base) {
  std::istringstream stream(text);
  std::int64_t value = 0;
  if (!(stream >> std::setbase(base) >> value) || !stream.eof()) {
    return std::nullopt;
  }
  return value;
}

// An output: stdout for no path or "-", none for "none", else the file.
class Output {
 public:
  bool Open(const std::optional<std::string>& path) {
    if (!path || *path == "-") {
      stream_ = &std::cout;
    } else if (*path != "none") {
      file_.open(*path);
      if (!file_) {
        return false;
      }
      stream_ = &file_;
    }
    return true;
  }
  std::ostream* Stream() { return stream_; }

 private:
  std::ofstream file_;
  std::ostream* stream_ = nullptr;
};

bool IsVsmRead(const std::string& event) {
  return event.find(" VSM READ ") != std::string::npos;
}

}  // namespace

namespace speak5220::testing {

int RunTms5220EmuCommand(std::span<char* const> arguments) {
  std::optional<std::string> samples_path;
  std::optional<std::string> events_path;
  std::optional<std::string> script;
  std::optional<std::int64_t> address;
  std::optional<std::int64_t> load_at;
  bool quiet_reads = false;
  BulkRun bulk;
  bool bulk_mode = false;

  std::vector<std::string> args;
  for (const char* argument : arguments.subspan(1)) {
    args.emplace_back(argument);
  }
  for (std::size_t i = 0; i < args.size(); ++i) {
    const std::string& a = args[i];
    auto next = [&]() -> std::optional<std::string> {
      if (i + 1 >= args.size()) {
        return std::nullopt;
      }
      return args[++i];
    };
    auto number = [&](int base) -> std::optional<std::int64_t> {
      std::optional<std::string> text = next();
      return text ? ParseNumber(*text, base) : std::nullopt;
    };
    if (a == "-s" || a == "-e" || a == "--image" || a == "--prior") {
      std::optional<std::string> value = next();
      if (!value) {
        return Usage();
      }
      if (a == "-s") {
        samples_path = value;
      } else if (a == "-e") {
        events_path = value;
      } else if (a == "--image") {
        bulk.image = *value;
        bulk_mode = true;
      } else {
        bulk.prior = *value;
      }
    } else if (a == "-q") {
      quiet_reads = true;
    } else if (a == "--external") {
      bulk.external = true;
    } else if (a == "--reset") {
      bulk.speak.reset = true;
    } else if (a == "--addr" || a == "--load-at") {
      std::optional<std::int64_t> value = number(16);
      if (!value || *value < 0) {
        return Usage();
      }
      (a == "--addr" ? address : load_at) = value;
    } else if (a == "--speak-at" || a == "--margin" || a == "--phase") {
      std::optional<std::int64_t> value = number(10);
      if (!value) {
        return Usage();
      }
      if (a == "--speak-at") {
        bulk.speak.speak_at = *value;
      } else if (a == "--margin") {
        bulk.speak.margin = *value;
      } else {
        bulk.phase = static_cast<int>(*value);
      }
    } else if ((a.size() > 1 && a[0] == '-') || script) {
      return Usage();
    } else {
      script = a;
    }
  }
  if (!script && !bulk_mode) {
    return Usage();
  }

  Output samples;
  Output events;
  if (!samples.Open(samples_path)) {
    return Fail("cannot write " + *samples_path);
  }
  if (!events.Open(events_path)) {
    return Fail("cannot write " + *events_path);
  }

  speak5220::core::StatusOr<EmulatorRun> run = EmulatorRun{};
  if (bulk_mode) {
    if (!address) {
      return Usage();
    }
    bulk.address = static_cast<std::uint32_t>(*address);
    bulk.load_at = static_cast<std::uint32_t>(load_at.value_or(*address));
    run = RunBulk(bulk);
  } else {
    run = RunScenarioScript(*script);
  }
  if (!run.IsOk()) {
    return Fail(run.GetStatus().Message());
  }

  if (std::ostream* out = samples.Stream()) {
    for (std::size_t n = 0; n < run->codes.size(); ++n) {
      *out << n << " " << n * emulator::Tms5220::kSlotsPerSample << " "
           << run->codes[n] << "\n";
    }
  }
  if (std::ostream* out = events.Stream()) {
    for (const std::string& event : run->events) {
      if (!quiet_reads || !IsVsmRead(event)) {
        *out << event << "\n";
      }
    }
  }
  std::cout.flush();
  return 0;
}

}  // namespace speak5220::testing
