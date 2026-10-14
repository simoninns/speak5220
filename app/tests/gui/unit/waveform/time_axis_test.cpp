/*
 * File:        time_axis_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of the waveform's mapping between samples and
 *              pixels: fit, zoom, scroll and their limits
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/time_axis.h"

#include "gtest/gtest.h"

namespace speak5220::app {
namespace {

constexpr int kWidth = 400;
const TimeExtent kExtent{.begin = -200.0, .end = 7800.0};

TEST(TimeAxisTest, SamplesAndPixelsMapBothWays) {
  const TimeAxis axis{.first = 100.0, .samples_per_pixel = 4.0};
  EXPECT_DOUBLE_EQ(axis.SampleToX(100.0), 0.0);
  EXPECT_DOUBLE_EQ(axis.SampleToX(500.0), 100.0);
  EXPECT_DOUBLE_EQ(axis.XToSample(100.0), 500.0);
  EXPECT_DOUBLE_EQ(axis.XToSample(axis.SampleToX(1234.5)), 1234.5);
}

TEST(TimeAxisTest, FitShowsTheWholeExtent) {
  const TimeAxis axis = FitAxis(kExtent, kWidth);
  EXPECT_DOUBLE_EQ(axis.first, -200.0);
  EXPECT_DOUBLE_EQ(axis.samples_per_pixel, 20.0);
  EXPECT_DOUBLE_EQ(axis.XToSample(kWidth), 7800.0);
}

TEST(TimeAxisTest, FitOfAFewSamplesStopsAtTheClosestZoom) {
  const TimeAxis axis = FitAxis({.begin = 0.0, .end = 10.0}, kWidth);
  EXPECT_DOUBLE_EQ(axis.samples_per_pixel, TimeAxis::kMinSamplesPerPixel);
}

TEST(TimeAxisTest, ZoomKeepsTheSampleUnderTheAnchor) {
  const TimeAxis fit = FitAxis(kExtent, kWidth);
  const double anchor_x = 300.0;
  const double anchored = fit.XToSample(anchor_x);
  const TimeAxis zoomed = ZoomAxis(fit, 4.0, anchor_x, kExtent, kWidth);
  EXPECT_DOUBLE_EQ(zoomed.samples_per_pixel, 5.0);
  EXPECT_DOUBLE_EQ(zoomed.XToSample(anchor_x), anchored);
}

TEST(TimeAxisTest, ZoomStopsAtTheClosestAndAtTheFit) {
  const TimeAxis fit = FitAxis(kExtent, kWidth);
  EXPECT_DOUBLE_EQ(ZoomAxis(fit, 1e6, 0.0, kExtent, kWidth).samples_per_pixel,
                   TimeAxis::kMinSamplesPerPixel);
  EXPECT_EQ(ZoomAxis(fit, 0.25, 200.0, kExtent, kWidth), fit);
}

TEST(TimeAxisTest, CentredZoomPutsTheSampleInTheMiddle) {
  const TimeAxis fit = FitAxis(kExtent, kWidth);
  const TimeAxis zoomed = ZoomAxisCentred(fit, 4.0, 1000.0, kExtent, kWidth);
  EXPECT_DOUBLE_EQ(zoomed.samples_per_pixel, 5.0);
  EXPECT_DOUBLE_EQ(zoomed.XToSample(kWidth / 2.0), 1000.0);
  // Near the start, the view stays inside the extent.
  EXPECT_DOUBLE_EQ(ZoomAxisCentred(fit, 4.0, -150.0, kExtent, kWidth).first,
                   -200.0);
}

TEST(TimeAxisTest, ScrollStaysInsideTheExtent) {
  const TimeAxis zoomed{.first = 0.0, .samples_per_pixel = 2.0};
  EXPECT_DOUBLE_EQ(ScrollAxis(zoomed, 50.0, kExtent, kWidth).first, 100.0);
  EXPECT_DOUBLE_EQ(ScrollAxis(zoomed, -1e6, kExtent, kWidth).first, -200.0);
  // The view ends at the extent's end: 7800 - 400 x 2.
  EXPECT_DOUBLE_EQ(ScrollAxis(zoomed, 1e6, kExtent, kWidth).first, 7000.0);
}

TEST(TimeAxisTest, ExtentNarrowerThanTheViewStartsAtItsLeft) {
  const TimeExtent narrow{.begin = 0.0, .end = 10.0};
  const TimeAxis axis =
      ClampAxis({.first = 5.0, .samples_per_pixel = 0.0625}, narrow, kWidth);
  EXPECT_DOUBLE_EQ(axis.first, 0.0);
}

}  // namespace
}  // namespace speak5220::app
