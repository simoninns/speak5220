/*
 * File:        waveform_view_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of the waveform view: placing samples by their
 *              offset, the frame under the pointer, zoom, clicks and keys
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/waveform_view.h"

#include <QCoreApplication>
#include <QEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPointF>
#include <QString>
#include <QWidget>
#include <optional>
#include <vector>

#include "core/lpc/frame.h"
#include "gtest/gtest.h"
#include "gui/waveform/time_axis.h"

namespace speak5220::app {
namespace {

using core::lpc::Frame;

constexpr int kWidth = 400;
constexpr int kHeight = 100;
// Where source sample 0 is in the encoded samples: the emulator's start
// latency and the encoder's lead-in.
constexpr int kOffset = 300;

// A left click at pixel `x`.
void Click(QWidget& widget, double x) {
  const QPointF at(x, kHeight / 2.0);
  QMouseEvent press(QEvent::MouseButtonPress, at, widget.mapToGlobal(at),
                    Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
  QCoreApplication::sendEvent(&widget, &press);
}

class WaveformViewTest : public ::testing::Test {
 protected:
  WaveformViewTest() {
    view_.resize(kWidth, kHeight);
    // Shown (offscreen), so resizing sends resize events.
    view_.show();
    view_.SetSamples(std::vector<float>(2000, 0.25F), kOffset);
  }

  WaveformView view_;
};

TEST_F(WaveformViewTest, FitsTheSamplesPlacedByTheirOffset) {
  EXPECT_EQ(view_.ContentExtent(),
            (TimeExtent{.begin = -300.0, .end = 1700.0}));
  EXPECT_DOUBLE_EQ(view_.Axis().first, -300.0);
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 5.0);
  // Encoded sample 300 is source sample 0.
  EXPECT_DOUBLE_EQ(view_.XOfSample(kOffset), view_.Axis().SampleToX(0.0));
}

TEST_F(WaveformViewTest, FrameUnderThePointerAllowsForTheStartLatency) {
  view_.SetFrames({Frame::Voiced(9, 31, {}), Frame::Unvoiced(5, {1, 2, 3, 4}),
                   Frame::Stop()});
  // Frame 1's stretch is encoded samples 325-524.
  EXPECT_EQ(view_.FrameAtX(view_.XOfSample(325.0)), 1);
  EXPECT_EQ(view_.FrameAtX(view_.XOfSample(320.0)), 0);
  EXPECT_EQ(view_.FrameAtX(view_.XOfSample(100.0)), std::nullopt);
  EXPECT_EQ(view_.FrameAtX(view_.XOfSample(800.0)), std::nullopt);
  EXPECT_EQ(view_.ToolTipAt(view_.XOfSample(400.0)),
            QStringLiteral("Frame 1: unvoiced, energy 5, K 1 2 3 4"));
  EXPECT_TRUE(view_.ToolTipAt(view_.XOfSample(100.0)).isEmpty());
}

TEST_F(WaveformViewTest, NoFramesNoHoverText) {
  EXPECT_EQ(view_.FrameAtX(200.0), std::nullopt);
}

TEST_F(WaveformViewTest, ZoomInAndFitAgain) {
  int changed = 0;
  QObject::connect(&view_, &WaveformView::AxisChanged,
                   [&changed] { ++changed; });
  view_.ZoomIn();
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 2.5);
  view_.ScrollBy(40.0);
  view_.Fit();
  EXPECT_EQ(view_.Axis(), FitAxis(view_.Extent(), kWidth));
  EXPECT_EQ(changed, 3);
}

TEST_F(WaveformViewTest, ZoomOutStopsAtTheFit) {
  const TimeAxis fit = view_.Axis();
  view_.ZoomOut();
  EXPECT_EQ(view_.Axis(), fit);
}

TEST_F(WaveformViewTest, FittedViewFitsAgainWhenResized) {
  view_.resize(2 * kWidth, kHeight);
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 2.5);
  view_.ZoomIn();
  view_.resize(kWidth, kHeight);
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 1.25);
}

TEST_F(WaveformViewTest, ClickAsksForTheMarkerAtThatSourceSample) {
  std::vector<double> requested;
  QObject::connect(
      &view_, &WaveformView::MarkerRequested,
      [&requested](double sample) { requested.push_back(sample); });
  Click(view_, 100.0);
  ASSERT_EQ(requested.size(), 1U);
  // Pixel 100 at 5 samples per pixel from source sample -300.
  EXPECT_DOUBLE_EQ(requested.front(), 200.0);
}

TEST_F(WaveformViewTest, SpaceAsksToPlayOrPause) {
  int requested = 0;
  QObject::connect(&view_, &WaveformView::PlayPauseRequested,
                   [&requested] { ++requested; });
  QKeyEvent press(QEvent::KeyPress, Qt::Key_Space, Qt::NoModifier,
                  QStringLiteral(" "));
  QCoreApplication::sendEvent(&view_, &press);
  EXPECT_EQ(requested, 1);
}

TEST_F(WaveformViewTest, ClearShowsThePlaceholderAndNoClicks) {
  view_.Clear(QStringLiteral("Not encoded yet"));
  EXPECT_FALSE(view_.HasSamples());
  EXPECT_EQ(view_.Placeholder(), QStringLiteral("Not encoded yet"));
  int requested = 0;
  QObject::connect(&view_, &WaveformView::MarkerRequested,
                   [&requested] { ++requested; });
  Click(view_, 100.0);
  EXPECT_EQ(requested, 0);
}

}  // namespace
}  // namespace speak5220::app
