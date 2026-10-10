/*
 * File:        status.h
 * Module:      util
 * Purpose:     Status and StatusOr<T>, the error values returned in place of
 *              exceptions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_UTIL_STATUS_H_
#define SPEAK5220_CORE_UTIL_STATUS_H_

#include <cassert>
#include <string>
#include <string_view>
#include <utility>
#include <variant>

namespace speak5220::core {

// The kind of failure a Status reports. Modelled on Abseil's codes, keeping
// only the ones the application uses.
enum class StatusCode {
  kOk,
  kInvalidArgument,     // the caller passed something malformed
  kOutOfRange,          // a value or size outside what is allowed
  kNotFound,            // a file, word or entry that does not exist
  kFailedPrecondition,  // the operation cannot run in the current state
  kResourceExhausted,   // a fixed capacity (a PHROM, a buffer) is full
  kDataLoss,            // input that is truncated or corrupt
  kUnavailable,         // an external resource could not be used
  kInternal,            // a broken invariant
};

// Returns the code's name, such as "INVALID_ARGUMENT".
std::string_view StatusCodeName(StatusCode code);

// The result of an operation that can fail: OK, or a code and a message.
// Status is a value type. It is thread-compatible: distinct objects may be
// used from different threads.
class [[nodiscard]] Status {
 public:
  // An OK status.
  Status() = default;
  // A status with `code` and a message for the user or the log. An OK code
  // ignores the message.
  Status(StatusCode code, std::string message);

  bool IsOk() const { return code_ == StatusCode::kOk; }
  StatusCode Code() const { return code_; }
  const std::string& Message() const { return message_; }

  // "OK", or the code's name followed by the message.
  std::string ToString() const;

  friend bool operator==(const Status& a, const Status& b) = default;

 private:
  StatusCode code_ = StatusCode::kOk;
  std::string message_;
};

// Shorthands for the codes above.
inline Status OkStatus() { return {}; }
Status InvalidArgumentError(std::string message);
Status OutOfRangeError(std::string message);
Status NotFoundError(std::string message);
Status FailedPreconditionError(std::string message);
Status ResourceExhaustedError(std::string message);
Status DataLossError(std::string message);
Status UnavailableError(std::string message);
Status InternalError(std::string message);

// Either a value of type T or a non-OK Status. Constructing one from an OK
// Status is a programming error: it asserts, and in release builds becomes
// an INTERNAL error. Thread-compatible, like Status.
template <typename T>
class [[nodiscard]] StatusOr {
 public:
  // Implicit, so a function returning StatusOr<T> can return a T or an
  // error Status directly, as with Abseil's StatusOr.
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(T value) : state_(std::in_place_index<1>, std::move(value)) {}
  // NOLINTNEXTLINE(google-explicit-constructor)
  StatusOr(Status status) : state_(std::in_place_index<0>, std::move(status)) {
    assert(!std::get<0>(state_).IsOk() && "StatusOr built from an OK Status");
    if (std::get<0>(state_).IsOk()) {
      std::get<0>(state_) =
          Status(StatusCode::kInternal, "StatusOr built from OK");
    }
  }

  bool IsOk() const { return state_.index() == 1; }
  const Status& GetStatus() const& {
    static const Status kOk;
    return IsOk() ? kOk : std::get<0>(state_);
  }
  Status GetStatus() && {
    return IsOk() ? Status() : std::move(std::get<0>(state_));
  }

  // The value. Calling these on an error is a programming error.
  const T& Value() const& { return std::get<1>(state_); }
  T& Value() & { return std::get<1>(state_); }
  T&& Value() && { return std::get<1>(std::move(state_)); }
  const T& operator*() const& { return Value(); }
  T& operator*() & { return Value(); }
  const T* operator->() const { return &Value(); }
  T* operator->() { return &Value(); }

 private:
  // Index 0: the error; index 1: the value.
  std::variant<Status, T> state_;
};

}  // namespace speak5220::core

#endif  // SPEAK5220_CORE_UTIL_STATUS_H_
