/*
 * File:        waveform_view_test.cpp
 * Module:      waveform tests
 * Purpose:     Unit tests of the waveform view: placing samples by their
 *              offset, the frame and hover text under the pointer, zoom,
 *              clicks and keys
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "gui/waveform/waveform_view.h"

#include <QCoreApplication>
#include <QEvent>
#include <QImage>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QObject>
#include <QPalette>
#include <QPixmap>
#include <QPointF>
#include <QString>
#include <QWidget>
#include <cstddef>
#include <optional>
#include <vector>

#include "core/lpc/frame.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "gui/support/test_palettes.h"
#include "gui/theme/colours.h"
#include "gui/waveform/time_axis.h"

namespace speak5220::app {
namespace {

using core::lpc::Frame;

constexpr int kWidth = 400;
constexpr int kHeight = 100;
// Where source sample 0 is in the encoded samples: the emulator's start
// latency and the encoder's lead-in.
constexpr int kOffset = 300;

// A left-button event of `type` at pixel `x`.
void SendMouse(QWidget& widget, QEvent::Type type, double x) {
  const QPointF at(x, kHeight / 2.0);
  const Qt::MouseButtons held =
      type == QEvent::MouseButtonRelease ? Qt::NoButton : Qt::LeftButton;
  const Qt::MouseButton button =
      type == QEvent::MouseMove ? Qt::NoButton : Qt::LeftButton;
  QMouseEvent event(type, at, widget.mapToGlobal(at), button, held,
                    Qt::NoModifier);
  QCoreApplication::sendEvent(&widget, &event);
}

// A left click at pixel `x`.
void Click(QWidget& widget, double x) {
  SendMouse(widget, QEvent::MouseButtonPress, x);
  SendMouse(widget, QEvent::MouseButtonRelease, x);
}

// A left-button drag from pixel `from` to `to`.
void Drag(QWidget& widget, double from, double to) {
  SendMouse(widget, QEvent::MouseButtonPress, from);
  SendMouse(widget, QEvent::MouseMove, (from + to) / 2.0);
  SendMouse(widget, QEvent::MouseMove, to);
  SendMouse(widget, QEvent::MouseButtonRelease, to);
}

// A request for a selection.
struct Requested {
  double begin = 0.0;
  double end = 0.0;
  bool finished = false;
};

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

TEST_F(WaveformViewTest, ShadedStretchesSayTheyAreGated) {
  // Source samples 500-900.
  view_.SetShading({{.begin = 500.0, .end = 900.0}});
  EXPECT_THAT(view_.ToolTipAt(view_.Axis().SampleToX(700.0)).toStdString(),
              ::testing::StartsWith("Gated"));
  EXPECT_TRUE(view_.ToolTipAt(view_.Axis().SampleToX(300.0)).isEmpty());
  EXPECT_TRUE(view_.ToolTipAt(view_.Axis().SampleToX(950.0)).isEmpty());
  // It draws, and clearing the view clears it.
  QImage image(view_.size(), QImage::Format_ARGB32);
  view_.render(&image);
  EXPECT_FALSE(image.isNull());
  view_.Clear(QStringLiteral("None"));
  EXPECT_TRUE(view_.Shading().empty());
}

TEST_F(WaveformViewTest, ShadingDimsTheWaveform) {
  QImage plain(view_.size(), QImage::Format_ARGB32);
  view_.render(&plain);
  view_.SetShading({{.begin = -300.0, .end = 1700.0}});
  QImage shaded(view_.size(), QImage::Format_ARGB32);
  view_.render(&shaded);
  EXPECT_NE(plain, shaded);
}

// Just right of source sample `sample`'s left edge, so it is under the
// pointer whatever the rounding.
double JustRightOf(const WaveformView& view, double sample) {
  return view.Axis().SampleToX(sample) + 0.1;
}

TEST_F(WaveformViewTest, HoverTextGivesTheTimeSampleAndAmplitude) {
  EXPECT_EQ(view_.HoverTextAt(JustRightOf(view_, 800.0)),
            QStringLiteral("0.100 s, sample 800, amplitude +0.250"));
  // Beyond the extent there is nothing to say.
  EXPECT_TRUE(view_.HoverTextAt(view_.Axis().SampleToX(1800.0)).isEmpty());
  view_.Clear(QStringLiteral("None"));
  EXPECT_TRUE(view_.HoverTextAt(200.0).isEmpty());
}

TEST_F(WaveformViewTest, HoverTextGivesTheRangeOfAPixelsSamples) {
  std::vector<float> samples(2000, 0.5F);
  for (std::size_t i = 1; i < samples.size(); i += 2) {
    samples[i] = -0.5F;
  }
  view_.SetSamples(samples, kOffset);
  // Fitted, each pixel covers 5 samples.
  EXPECT_EQ(view_.HoverTextAt(JustRightOf(view_, 800.0)),
            QStringLiteral("0.100 s, sample 800, amplitude -0.500 to +0.500"));
  // Zoomed in to a sample's width or more, it gives just that one.
  view_.SetAxis({.first = 800.0, .samples_per_pixel = 0.5});
  EXPECT_EQ(view_.HoverTextAt(JustRightOf(view_, 801.0)),
            QStringLiteral("0.100 s, sample 801, amplitude -0.500"));
}

TEST_F(WaveformViewTest, HoverTextAddsTheFrameOrThatItIsGated) {
  view_.SetFrames({Frame::Voiced(9, 31, {}), Frame::Unvoiced(5, {1, 2, 3, 4}),
                   Frame::Stop()});
  // Encoded sample 420 is source sample 120, in frame 1.
  EXPECT_EQ(view_.HoverTextAt(JustRightOf(view_, 120.0)),
            QStringLiteral("0.015 s, sample 120, amplitude +0.250 · Frame 1: "
                           "unvoiced, energy 5, K 1 2 3 4"));
  view_.SetFrames({});
  view_.SetShading({{.begin = 500.0, .end = 900.0}});
  EXPECT_EQ(view_.HoverTextAt(JustRightOf(view_, 800.0)),
            QStringLiteral("0.100 s, sample 800, amplitude +0.250 · gated"));
}

TEST_F(WaveformViewTest, HoverTextFollowsThePointerUntilItLeaves) {
  std::vector<QString> texts;
  QObject::connect(&view_, &WaveformView::HoverTextChanged,
                   [&texts](const QString& text) { texts.push_back(text); });
  SendMouse(view_, QEvent::MouseMove, JustRightOf(view_, 800.0));
  ASSERT_EQ(texts.size(), 1U);
  EXPECT_EQ(texts.back(), view_.HoverText());
  EXPECT_THAT(texts.back().toStdString(),
              ::testing::StartsWith("0.100 s, sample 800"));
  // What is under the pointer changes as the view zooms.
  view_.ZoomIn();
  ASSERT_EQ(texts.size(), 2U);
  EXPECT_THAT(texts.back().toStdString(),
              ::testing::Not(::testing::StartsWith("0.100 s, sample 800")));
  QEvent leave(QEvent::Leave);
  QCoreApplication::sendEvent(&view_, &leave);
  ASSERT_EQ(texts.size(), 3U);
  EXPECT_TRUE(texts.back().isEmpty());
  EXPECT_TRUE(view_.HoverText().isEmpty());
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

TEST_F(WaveformViewTest, ClickAsksForTheMarkerOnlyWhenReleased) {
  int requested = 0;
  QObject::connect(&view_, &WaveformView::MarkerRequested,
                   [&requested] { ++requested; });
  SendMouse(view_, QEvent::MouseButtonPress, 100.0);
  EXPECT_EQ(requested, 0);
  SendMouse(view_, QEvent::MouseButtonRelease, 100.0);
  EXPECT_EQ(requested, 1);
}

TEST_F(WaveformViewTest, DragAsksForTheStretchDraggedOverToBeSelected) {
  std::vector<Requested> requested;
  int markers = 0;
  QObject::connect(&view_, &WaveformView::SelectionRequested,
                   [&requested](double begin, double end, bool finished) {
                     requested.push_back({begin, end, finished});
                   });
  QObject::connect(&view_, &WaveformView::MarkerRequested,
                   [&markers] { ++markers; });
  // Right to left: begin is still before end. Pixels 100 and 60 are source
  // samples 200 and 0.
  Drag(view_, 100.0, 60.0);
  ASSERT_EQ(requested.size(), 3U);
  EXPECT_FALSE(requested.front().finished);
  EXPECT_DOUBLE_EQ(requested.back().begin, 0.0);
  EXPECT_DOUBLE_EQ(requested.back().end, 200.0);
  EXPECT_TRUE(requested.back().finished);
  EXPECT_EQ(markers, 0);
}

TEST_F(WaveformViewTest, EscapeAsksToClearTheSelection) {
  int requested = 0;
  QObject::connect(&view_, &WaveformView::SelectionClearRequested,
                   [&requested] { ++requested; });
  QKeyEvent press(QEvent::KeyPress, Qt::Key_Escape, Qt::NoModifier);
  QCoreApplication::sendEvent(&view_, &press);
  EXPECT_EQ(requested, 0);
  view_.SetSelection(TimeExtent{.begin = 100.0, .end = 200.0});
  QCoreApplication::sendEvent(&view_, &press);
  EXPECT_EQ(requested, 1);
}

TEST_F(WaveformViewTest, ZoomCentresOnTheMarker) {
  view_.SetMarker(1000);
  view_.ZoomIn();
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 2.5);
  EXPECT_DOUBLE_EQ(view_.Axis().XToSample(kWidth / 2.0), 1000.0);
  view_.ZoomIn();
  EXPECT_DOUBLE_EQ(view_.Axis().XToSample(kWidth / 2.0), 1000.0);
  view_.ZoomOut();
  EXPECT_DOUBLE_EQ(view_.Axis().XToSample(kWidth / 2.0), 1000.0);
}

TEST_F(WaveformViewTest, FirstZoomInShowsJustTheSelectionThenStepsInIt) {
  view_.SetMarker(0);
  // 200 samples in 400 pixels: half a sample per pixel.
  view_.SetSelection(TimeExtent{.begin = 1000.0, .end = 1200.0});
  view_.ZoomIn();
  EXPECT_DOUBLE_EQ(view_.Axis().first, 1000.0);
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 0.5);
  EXPECT_FALSE(view_.IsFitted());
  view_.ZoomIn();
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 0.25);
  EXPECT_DOUBLE_EQ(view_.Axis().XToSample(kWidth / 2.0), 1100.0);
  view_.ZoomOut();
  view_.ZoomOut();
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 1.0);
  EXPECT_DOUBLE_EQ(view_.Axis().XToSample(kWidth / 2.0), 1100.0);
}

TEST_F(WaveformViewTest, SelectionWiderThanTheViewIsZoomedAbout) {
  view_.ZoomIn();
  view_.ZoomIn();
  // At 1.25 samples per pixel, closer than the selection's fit of 4.
  view_.SetSelection(TimeExtent{.begin = 0.0, .end = 1600.0});
  view_.ZoomIn();
  EXPECT_DOUBLE_EQ(view_.Axis().samples_per_pixel, 0.625);
  EXPECT_DOUBLE_EQ(view_.Axis().XToSample(kWidth / 2.0), 800.0);
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

// The backdrop (the logo, with no phrase) is drawn under the placeholder,
// and goes when a waveform is shown or the view is cleared without one.
TEST_F(WaveformViewTest, BackdropShowsOnlyWithNoWaveform) {
  QPixmap backdrop(40, 20);
  backdrop.fill(Qt::white);
  view_.Clear(QStringLiteral("Select one phrase to see it"), backdrop);
  EXPECT_TRUE(view_.HasBackdrop());
  QImage drawn(view_.size(), QImage::Format_ARGB32);
  view_.render(&drawn);
  EXPECT_NE(drawn.pixelColor(kWidth / 2, kHeight / 2), drawn.pixelColor(2, 2));

  view_.SetSamples(std::vector<float>(2000, 0.25F), kOffset);
  EXPECT_FALSE(view_.HasBackdrop());
  view_.Clear(QStringLiteral("Not encoded yet"));
  EXPECT_FALSE(view_.HasBackdrop());
}

TEST_F(WaveformViewTest, DrawsOnBlackWithTheDarkColoursInEitherScheme) {
  for (const QPalette& palette :
       {testing::LightPalette(), testing::DarkPalette()}) {
    view_.setPalette(palette);
    QImage drawn(view_.size(), QImage::Format_ARGB32);
    view_.render(&drawn);
    // Above the wave, which is at a quarter of full scale.
    EXPECT_EQ(drawn.pixelColor(kWidth / 2, 2), QColor(Qt::black));
    // The wave, somewhere down the middle column.
    bool wave_found = false;
    for (int y = 0; y < kHeight && !wave_found; ++y) {
      wave_found = drawn.pixelColor(kWidth / 2, y) ==
                   ColoursFor(testing::DarkPalette()).wave;
    }
    EXPECT_TRUE(wave_found);
  }
}

}  // namespace
}  // namespace speak5220::app
