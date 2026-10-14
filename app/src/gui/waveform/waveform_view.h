/*
 * File:        waveform_view.h
 * Module:      waveform
 * Purpose:     Draws a waveform with its play-start marker and playhead,
 *              and the frames over the encoded audio; zoom and scroll
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_WAVEFORM_VIEW_H_
#define SPEAK5220_GUI_WAVEFORM_WAVEFORM_VIEW_H_

#include <QPalette>
#include <QPixmap>
#include <QSize>
#include <QString>
#include <QWidget>
#include <cstdint>
#include <optional>
#include <vector>

#include "core/lpc/frame.h"
#include "gui/theme/colours.h"
#include "gui/waveform/peak_cache.h"
#include "gui/waveform/time_axis.h"

class QEvent;
class QKeyEvent;
class QMouseEvent;
class QPainter;
class QPaintEvent;
class QResizeEvent;
class QWheelEvent;

namespace speak5220::app {

// Plan §5: one waveform pane's drawing. Time is counted in samples of the
// phrase's source audio (TimeAxis); this view's own samples are placed by
// their source offset, so the encoded audio lines up with the original.
// It draws, from the back: the frames' kinds and the 25 ms frame grid
// (encoded audio only), the zero line, the waveform (one line per pixel
// from the PeakCache, each reaching the last sample of the pixel before so
// the trace is unbroken; or, closer than 4 samples per pixel, the samples
// joined), the shaded stretches (dimmed and hatched: what the noise gate
// turns down, on the preprocessed audio), the play-start marker and the
// playhead.
//
// A click asks for the marker there (MarkerRequested); Space asks to play
// or pause; Ctrl+wheel zooms about the pointer, the wheel scrolls; hovering
// over a frame shows its indices. With no waveform it shows a placeholder,
// under a faint backdrop if it has one. The view stays fitted to its extent
// until zoomed or scrolled. It is drawn dark, on black, in either scheme
// (WaveformPalette(), ColoursFor()), unless set to follow the
// application's scheme (SetWaveformColours()). GUI thread only.
class WaveformView : public QWidget {
  Q_OBJECT

 public:
  // The factor of one zoom step, by button or wheel notch.
  static constexpr double kZoomStep = 2.0;

  explicit WaveformView(QWidget* parent = nullptr);

  // Shows `samples` (8 kHz, full scale +-1), with source sample s at
  // samples[s + source_offset], and fits the view to them.
  void SetSamples(std::vector<float> samples, int source_offset);
  // Shows no waveform, but `placeholder` in its place, under a faint
  // `backdrop` (such as the logo) if one is given. Showing samples takes
  // both away.
  void Clear(const QString& placeholder, QPixmap backdrop = QPixmap());
  bool HasSamples() const { return peaks_.Size() > 0; }
  bool HasBackdrop() const { return !backdrop_.isNull(); }
  int SourceOffset() const { return source_offset_; }
  QString Placeholder() const { return placeholder_; }

  // Stretches to shade, in source samples, or none; hovering over one
  // says it is gated.
  void SetShading(std::vector<TimeExtent> spans);
  const std::vector<TimeExtent>& Shading() const { return shading_; }

  // The frames over the samples (the encoded audio), or none.
  void SetFrames(std::vector<core::lpc::Frame> frames);
  void SetFrameGridShown(bool shown);
  bool FrameGridShown() const { return frame_grid_; }

  // Draws the waveform dark in either scheme (the default) or in the
  // application's scheme (WaveformPaletteFor()).
  void SetWaveformColours(WaveformColours choice);
  WaveformColours Colours() const { return colour_choice_; }
  // The palette the waveform is drawn with.
  const QPalette& WavePalette() const { return wave_palette_; }

  // The source samples this view's samples cover.
  TimeExtent ContentExtent() const;
  // The span the view scrolls over: its own content, or the union with
  // the other pane's when their time axes are linked.
  void SetExtent(const TimeExtent& extent);
  const TimeExtent& Extent() const { return extent_; }

  const TimeAxis& Axis() const { return axis_; }
  // Sets the axis, clamped to the extent; AxisChanged() if it changed.
  void SetAxis(const TimeAxis& axis);
  void ZoomIn();
  void ZoomOut();
  void Fit();
  // Whether the view shows its whole extent, as Fit() left it; it fits
  // again when resized.
  bool IsFitted() const { return fitted_; }
  // Scrolls by `pixels` (positive is later).
  void ScrollBy(double pixels);

  // The play-start marker and the playhead, in source samples.
  void SetMarker(std::optional<int> source_sample);
  std::optional<int> Marker() const { return marker_; }
  void SetPlayhead(std::optional<double> source_sample);
  std::optional<double> Playhead() const { return playhead_; }

  // The x of sample `index` of this view's own samples.
  double XOfSample(double index) const;
  // The frame under pixel `x`, or none.
  std::optional<int> FrameAtX(double x) const;
  // The hover text at pixel `x`: the frame's description, that the
  // stretch is gated, or empty.
  QString ToolTipAt(double x) const;

  QSize sizeHint() const override;
  QSize minimumSizeHint() const override;

 signals:
  void AxisChanged();
  // A click: the marker should move to this source sample.
  void MarkerRequested(double source_sample);
  // Space: play or pause this pane.
  void PlayPauseRequested();

 protected:
  bool event(QEvent* event) override;
  void changeEvent(QEvent* event) override;
  void keyPressEvent(QKeyEvent* event) override;
  void mousePressEvent(QMouseEvent* event) override;
  void paintEvent(QPaintEvent* event) override;
  void resizeEvent(QResizeEvent* event) override;
  void wheelEvent(QWheelEvent* event) override;

 private:
  void Apply(const TimeAxis& axis, bool fitted);
  double SampleY(float value) const;
  void PaintFrames(QPainter& painter) const;
  void PaintWave(QPainter& painter) const;
  void PaintShading(QPainter& painter) const;
  void PaintMarks(QPainter& painter) const;
  // The backdrop, faint and centred, and the placeholder under it.
  void PaintBackdrop(QPainter& painter) const;

  PeakCache peaks_;
  int source_offset_ = 0;
  QString placeholder_;
  QPixmap backdrop_;
  std::vector<core::lpc::Frame> frames_;
  std::vector<TimeExtent> shading_;
  bool frame_grid_ = true;
  TimeExtent extent_;
  TimeAxis axis_;
  // Fitted views fit again when resized.
  bool fitted_ = true;
  std::optional<int> marker_;
  std::optional<double> playhead_;
  // The palette the waveform is drawn with (WaveformPaletteFor()), and its
  // colours.
  WaveformColours colour_choice_ = WaveformColours::kDark;
  QPalette wave_palette_;
  ThemeColours colours_;
};

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_WAVEFORM_VIEW_H_
