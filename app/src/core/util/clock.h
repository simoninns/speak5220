/*
 * File:        clock.h
 * Module:      util
 * Purpose:     A monotonic clock: the interface and the system's steady
 *              clock
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_CLOCK_H_
#define SPEAK5220_CORE_UTIL_CLOCK_H_

#include <cstdint>

namespace speak5220::core {

// Tells the time, for code that measures intervals (the playhead, the
// debounce of re-encoding), so tests can use a fake clock.
// Implementations are thread-safe.
class IClock {
 public:
  virtual ~IClock() = default;

  // Microseconds since an arbitrary origin; never goes backwards.
  virtual std::int64_t NowMicroseconds() const = 0;
};

// IClock over std::chrono::steady_clock. Stateless; thread-safe.
class SteadyClock : public IClock {
 public:
  std::int64_t NowMicroseconds() const override;
};

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_CLOCK_H_
