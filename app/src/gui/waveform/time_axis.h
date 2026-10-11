/*
 * File:        time_axis.h
 * Module:      waveform
 * Purpose:     The waveform view's mapping between samples and pixels, with
 *              zoom, scroll and fit
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_TIME_AXIS_H_
#define SPEAK5220_GUI_WAVEFORM_TIME_AXIS_H_

namespace speak5220::app {

// A span of source samples, [begin, end): what a view can scroll over.
struct TimeExtent {
  double begin = 0.0;
  double end = 0.0;

  friend bool operator==(const TimeExtent& a, const TimeExtent& b) = default;
};

// Plan §5: which source sample is at each pixel. Both waveform panes count
// time in samples of the phrase's source audio, so with the same axis the
// same sound is at the same place in both (the encoded pane places its
// own samples by the encoded audio's offset). A plain value type.
struct TimeAxis {
  // The closest zoom: 16 pixels per sample.
  static constexpr double kMinSamplesPerPixel = 1.0 / 16.0;

  // The source sample at pixel 0 (the left edge).
  double first = 0.0;
  double samples_per_pixel = 1.0;

  double SampleToX(double sample) const {
    return (sample - first) / samples_per_pixel;
  }
  double XToSample(double x) const { return first + (x * samples_per_pixel); }

  friend bool operator==(const TimeAxis& a, const TimeAxis& b) = default;
};

// The axis showing all of `extent` in `width` pixels.
TimeAxis FitAxis(const TimeExtent& extent, int width);

// `axis` with its zoom limited to between kMinSamplesPerPixel and the fit
// of `extent`, and scrolled so the view stays inside `extent` (an extent
// narrower than the view starts at its left edge).
TimeAxis ClampAxis(const TimeAxis& axis, const TimeExtent& extent, int width);

// `axis` zoomed in by `factor` (above 1 zooms in, below 1 out), keeping
// the sample at pixel `anchor_x` where it is, then clamped.
TimeAxis ZoomAxis(const TimeAxis& axis, double factor, double anchor_x,
                  const TimeExtent& extent, int width);

// `axis` scrolled by `pixels` (positive moves later in time), clamped.
TimeAxis ScrollAxis(const TimeAxis& axis, double pixels,
                    const TimeExtent& extent, int width);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_TIME_AXIS_H_
