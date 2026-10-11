/*
 * File:        tms5220_config.cpp
 * Module:      emulator
 * Purpose:     The timing choices the datasheet leaves open, one field per
 *              "DATASHEET GAP" of the emulator
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/emulator/tms5220_config.h"

#include <array>
#include <string>
#include <string_view>

namespace speak5220::core::emulator {

Status SetConfigParameter(Tms5220Config& config, std::string_view name,
                          int value) {
  struct Field {
    std::string_view name;
    int Tms5220Config::* member;
  };
  static constexpr std::array<Field, 16> kFields = {{
      {.name = "cmd_latch", .member = &Tms5220Config::cmd_latch},
      {.name = "fifo_latch", .member = &Tms5220Config::fifo_latch},
      {.name = "read_ready", .member = &Tms5220Config::read_ready},
      {.name = "vsm_delay", .member = &Tms5220Config::vsm_delay},
      {.name = "vsm_spacing", .member = &Tms5220Config::vsm_spacing},
      {.name = "branch_ready", .member = &Tms5220Config::branch_ready},
      {.name = "speak_lead", .member = &Tms5220Config::speak_lead},
      {.name = "ts_set_delay", .member = &Tms5220Config::ts_set_delay},
      {.name = "ts_clear_stop", .member = &Tms5220Config::ts_clear_stop},
      {.name = "ts_clear_first_stop",
       .member = &Tms5220Config::ts_clear_first_stop},
      {.name = "noise_rel_speak", .member = &Tms5220Config::noise_rel_speak},
      {.name = "noise_rel_se", .member = &Tms5220Config::noise_rel_se},
      {.name = "noise_hold_steps", .member = &Tms5220Config::noise_hold_steps},
      {.name = "se_shift", .member = &Tms5220Config::se_shift},
      {.name = "frame_read_slot", .member = &Tms5220Config::frame_read_slot},
      {.name = "reset_nibble", .member = &Tms5220Config::reset_nibble},
  }};
  for (const Field& field : kFields) {
    if (field.name == name) {
      config.*field.member = value;
      return OkStatus();
    }
  }
  return InvalidArgumentError("unknown emulator parameter \"" +
                              std::string(name) + "\"");
}

}  // namespace speak5220::core::emulator
