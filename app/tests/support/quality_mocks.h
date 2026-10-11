/*
 * File:        quality_mocks.h
 * Module:      test support
 * Purpose:     gMock mocks of the quality interfaces: a quality metric and
 *              the anchor score
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_QUALITY_MOCKS_H_
#define SPEAK5220_TESTS_SUPPORT_QUALITY_MOCKS_H_

#include <span>

#include "core/encoder/encoder.h"
#include "core/quality/anchor_vocoder.h"
#include "core/quality/quality_metric.h"
#include "core/util/status.h"
#include "gmock/gmock.h"

namespace speak5220::testing {

class MockQualityMetric : public core::quality::IQualityMetric {
 public:
  MOCK_METHOD(core::StatusOr<double>, Measure,
              (std::span<const double> source,
               const core::encoder::EncodeResult& encoding),
              (override));
  MOCK_METHOD(core::quality::MetricDirection, Direction, (), (const, override));
};

class MockAnchorScorer : public core::quality::IAnchorScorer {
 public:
  MOCK_METHOD(core::StatusOr<double>, Score, (std::span<const double> source),
              (override));
};

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_QUALITY_MOCKS_H_
