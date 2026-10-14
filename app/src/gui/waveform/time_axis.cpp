/*
 * File:        time_axis.cpp
 * Module:      waveform
 * Purpose:     The waveform view's mapping between samples and pixels, with
 *              zoom, scroll and fit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/time_axis.h"

#include <algorithm>

namespace speak5220::app {

namespace {

// The width used for an empty or unshown view, in pixels.
constexpr int kMinWidth = 1;

double Width(int width) {
  return static_cast<double>(std::max(width, kMinWidth));
}

}  // namespace

TimeAxis FitAxis(const TimeExtent& extent, int width) {
  const double span = std::max(extent.end - extent.begin, 0.0);
  return {.first = extent.begin,
          .samples_per_pixel =
              std::max(span / Width(width), TimeAxis::kMinSamplesPerPixel)};
}

TimeAxis ClampAxis(const TimeAxis& axis, const TimeExtent& extent, int width) {
  const double fit = FitAxis(extent, width).samples_per_pixel;
  TimeAxis clamped = axis;
  clamped.samples_per_pixel =
      std::clamp(axis.samples_per_pixel, TimeAxis::kMinSamplesPerPixel, fit);
  const double last_first =
      extent.end - (Width(width) * clamped.samples_per_pixel);
  clamped.first =
      std::clamp(axis.first, extent.begin, std::max(extent.begin, last_first));
  return clamped;
}

TimeAxis ZoomAxis(const TimeAxis& axis, double factor, double anchor_x,
                  const TimeExtent& extent, int width) {
  const double anchor = axis.XToSample(anchor_x);
  const double fit = FitAxis(extent, width).samples_per_pixel;
  TimeAxis zoomed = axis;
  zoomed.samples_per_pixel = std::clamp(axis.samples_per_pixel / factor,
                                        TimeAxis::kMinSamplesPerPixel, fit);
  zoomed.first = anchor - (anchor_x * zoomed.samples_per_pixel);
  return ClampAxis(zoomed, extent, width);
}

TimeAxis ZoomAxisCentred(const TimeAxis& axis, double factor, double centre,
                         const TimeExtent& extent, int width) {
  const double fit = FitAxis(extent, width).samples_per_pixel;
  TimeAxis zoomed = axis;
  zoomed.samples_per_pixel = std::clamp(axis.samples_per_pixel / factor,
                                        TimeAxis::kMinSamplesPerPixel, fit);
  zoomed.first = centre - (Width(width) / 2.0 * zoomed.samples_per_pixel);
  return ClampAxis(zoomed, extent, width);
}

TimeAxis ScrollAxis(const TimeAxis& axis, double pixels,
                    const TimeExtent& extent, int width) {
  TimeAxis scrolled = axis;
  scrolled.first += pixels * axis.samples_per_pixel;
  return ClampAxis(scrolled, extent, width);
}

}  // namespace speak5220::app
