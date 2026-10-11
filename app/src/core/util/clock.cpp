/*
 * File:        clock.cpp
 * Module:      util
 * Purpose:     A monotonic clock: the interface and the system's steady
 *              clock
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/clock.h"

#include <chrono>
#include <cstdint>

namespace speak5220::core {

std::int64_t SteadyClock::NowMicroseconds() const {
  return std::chrono::duration_cast<std::chrono::microseconds>(
             std::chrono::steady_clock::now().time_since_epoch())
      .count();
}

}  // namespace speak5220::core
