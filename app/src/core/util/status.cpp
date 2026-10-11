/*
 * File:        status.cpp
 * Module:      util
 * Purpose:     Status and StatusOr<T>, the error values returned in place of
 *              exceptions
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/status.h"

#include <string>
#include <string_view>
#include <utility>

namespace speak5220::core {

std::string_view StatusCodeName(StatusCode code) {
  switch (code) {
    case StatusCode::kOk:
      return "OK";
    case StatusCode::kInvalidArgument:
      return "INVALID_ARGUMENT";
    case StatusCode::kOutOfRange:
      return "OUT_OF_RANGE";
    case StatusCode::kNotFound:
      return "NOT_FOUND";
    case StatusCode::kFailedPrecondition:
      return "FAILED_PRECONDITION";
    case StatusCode::kResourceExhausted:
      return "RESOURCE_EXHAUSTED";
    case StatusCode::kDataLoss:
      return "DATA_LOSS";
    case StatusCode::kUnavailable:
      return "UNAVAILABLE";
    case StatusCode::kInternal:
      return "INTERNAL";
  }
  return "UNKNOWN";
}

Status::Status(StatusCode code, std::string message) : code_(code) {
  if (code_ != StatusCode::kOk) {
    message_ = std::move(message);
  }
}

std::string Status::ToString() const {
  if (IsOk()) {
    return "OK";
  }
  std::string text(StatusCodeName(code_));
  text += ": ";
  text += message_;
  return text;
}

Status InvalidArgumentError(std::string message) {
  return {StatusCode::kInvalidArgument, std::move(message)};
}
Status OutOfRangeError(std::string message) {
  return {StatusCode::kOutOfRange, std::move(message)};
}
Status NotFoundError(std::string message) {
  return {StatusCode::kNotFound, std::move(message)};
}
Status FailedPreconditionError(std::string message) {
  return {StatusCode::kFailedPrecondition, std::move(message)};
}
Status ResourceExhaustedError(std::string message) {
  return {StatusCode::kResourceExhausted, std::move(message)};
}
Status DataLossError(std::string message) {
  return {StatusCode::kDataLoss, std::move(message)};
}
Status UnavailableError(std::string message) {
  return {StatusCode::kUnavailable, std::move(message)};
}
Status InternalError(std::string message) {
  return {StatusCode::kInternal, std::move(message)};
}

}  // namespace speak5220::core
