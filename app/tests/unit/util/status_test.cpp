/*
 * File:        status_test.cpp
 * Module:      util tests
 * Purpose:     Unit tests of Status and StatusOr: codes, messages and
 *              propagation
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/util/status.h"

#include <memory>
#include <string>
#include <utility>

#include "gtest/gtest.h"

namespace speak5220::core {
namespace {

StatusOr<int> ParsePositive(int value) {
  if (value <= 0) {
    return OutOfRangeError("not positive: " + std::to_string(value));
  }
  return value;
}

// Passes an error from a callee up unchanged, as callers are expected to.
StatusOr<int> DoubledPositive(int value) {
  StatusOr<int> parsed = ParsePositive(value);
  if (!parsed.IsOk()) {
    return parsed.GetStatus();
  }
  return *parsed * 2;
}

Status CheckAll(int a, int b) {
  if (Status status = DoubledPositive(a).GetStatus(); !status.IsOk()) {
    return status;
  }
  return DoubledPositive(b).GetStatus();
}

TEST(StatusTest, DefaultIsOk) {
  const Status status;
  EXPECT_TRUE(status.IsOk());
  EXPECT_EQ(status.Code(), StatusCode::kOk);
  EXPECT_EQ(status.ToString(), "OK");
  EXPECT_EQ(status, OkStatus());
}

TEST(StatusTest, ErrorKeepsCodeAndMessage) {
  const Status status = NotFoundError("no such word");
  EXPECT_FALSE(status.IsOk());
  EXPECT_EQ(status.Code(), StatusCode::kNotFound);
  EXPECT_EQ(status.Message(), "no such word");
  EXPECT_EQ(status.ToString(), "NOT_FOUND: no such word");
}

TEST(StatusTest, OkCodeDropsMessage) {
  EXPECT_EQ(Status(StatusCode::kOk, "ignored").Message(), "");
}

TEST(StatusTest, EachShorthandGivesItsCode) {
  EXPECT_EQ(InvalidArgumentError("").Code(), StatusCode::kInvalidArgument);
  EXPECT_EQ(OutOfRangeError("").Code(), StatusCode::kOutOfRange);
  EXPECT_EQ(FailedPreconditionError("").Code(),
            StatusCode::kFailedPrecondition);
  EXPECT_EQ(ResourceExhaustedError("").Code(), StatusCode::kResourceExhausted);
  EXPECT_EQ(DataLossError("").Code(), StatusCode::kDataLoss);
  EXPECT_EQ(UnavailableError("").Code(), StatusCode::kUnavailable);
  EXPECT_EQ(InternalError("").Code(), StatusCode::kInternal);
}

TEST(StatusOrTest, HoldsValue) {
  const StatusOr<int> result = ParsePositive(21);
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(*result, 21);
  EXPECT_TRUE(result.GetStatus().IsOk());
}

TEST(StatusOrTest, HoldsError) {
  const StatusOr<int> result = ParsePositive(-3);
  ASSERT_FALSE(result.IsOk());
  EXPECT_EQ(result.GetStatus().Code(), StatusCode::kOutOfRange);
  EXPECT_EQ(result.GetStatus().Message(), "not positive: -3");
}

TEST(StatusOrTest, PropagatesErrorThroughCallers) {
  const StatusOr<int> doubled = DoubledPositive(0);
  ASSERT_FALSE(doubled.IsOk());
  EXPECT_EQ(doubled.GetStatus(), OutOfRangeError("not positive: 0"));
  EXPECT_EQ(CheckAll(1, -7), OutOfRangeError("not positive: -7"));
  EXPECT_TRUE(CheckAll(1, 2).IsOk());
}

TEST(StatusOrTest, MovesOutMoveOnlyValue) {
  StatusOr<std::unique_ptr<int>> result = std::make_unique<int>(5);
  ASSERT_TRUE(result.IsOk());
  const std::unique_ptr<int> value = std::move(result).Value();
  EXPECT_EQ(*value, 5);
}

}  // namespace
}  // namespace speak5220::core
