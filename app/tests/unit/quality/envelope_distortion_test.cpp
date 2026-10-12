/*
 * File:        envelope_distortion_test.cpp
 * Module:      quality tests
 * Purpose:     Unit tests of the spectral envelope measure: the distortion,
 *              its outliers, the mask and silent frames
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/envelope_distortion.h"

#include <cmath>
#include <cstddef>
#include <span>
#include <vector>

#include "core/quality/lpc_envelope.h"
#include "core/quality/signal_preparation.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/quality_mocks.h"

namespace speak5220::core::quality {
namespace {

using ::testing::_;
using ::testing::AllOf;
using ::testing::Field;
using ::testing::Invoke;
using ::testing::Return;
using ::testing::SizeIs;

constexpr int kBins = kEnvelopeFft / 2 + 1;

// The original at `original` and the output at `output` throughout, one
// envelope frame per entry of `speech`, whose centre is speech when the
// entry says.
PreparedSignals Signals(const std::vector<bool>& speech, double original,
                        double output) {
  PreparedSignals p;
  const std::size_t n = speech.size() * kEnvelopeFrame;
  p.original.assign(n, original);
  p.output.assign(n, output);
  p.speech.assign(n / kVoiceActivityFrame, false);
  for (std::size_t k = 0; k < speech.size(); ++k) {
    p.speech[(k * kEnvelopeFrame + kEnvelopeFrame / 2) / kVoiceActivityFrame] =
        speech[k];
  }
  return p;
}

// An analysis whose fit records the frame's middle value in a[0], and whose
// envelope is flat at `db_per_unit` x a[0] dB; a silent frame has no fit.
class FakeAnalysis {
 public:
  explicit FakeAnalysis(double db_per_unit) {
    ON_CALL(lpc, Analyse(_, kEnvelopeOrder))
        .WillByDefault(
            Invoke([](std::span<const double> frame, int) -> StatusOr<LpcFit> {
              if (frame[kEnvelopeFrame / 2] == 0.0) {
                return FailedPreconditionError("the frame is silent");
              }
              std::vector<double> a(kEnvelopeOrder, 0.0);
              a[0] = frame[kEnvelopeFrame / 2];
              return LpcFit{.gain = 3.0, .a = a};
            }));
    ON_CALL(lpc, EnvelopeDb(_, kEnvelopeFft))
        .WillByDefault(Invoke([db_per_unit](const LpcFit& fit, int) {
          return StatusOr<std::vector<double>>(
              std::vector<double>(kBins, db_per_unit * fit.a[0]));
        }));
  }

  ::testing::NiceMock<testing::MockLpcAnalysis> lpc;
};

TEST(EnvelopeDistortionTest, LogSpectralDistortionIsTheRmsDifference) {
  const std::vector<double> a = {0.0, 1.0, 2.0, 3.0};
  const std::vector<double> b = {9.0, 4.0, 2.0, 7.0};
  // Bins 1..3: differences 3, 0 and 4.
  EXPECT_NEAR(LogSpectralDistortion(a, b, 1, 3), std::sqrt(25.0 / 3.0), 1e-12);
  EXPECT_EQ(LogSpectralDistortion(a, a, 0, 3), 0.0);
}

TEST(EnvelopeDistortionTest, IdenticalSignalsHaveNoDistortion) {
  FakeAnalysis analysis(10.0);
  const StatusOr<EnvelopeDistortion> d = MeasureEnvelopeDistortion(
      Signals({true, true, true}, 0.5, 0.5), analysis.lpc);
  ASSERT_TRUE(d.IsOk()) << d.GetStatus().ToString();
  EXPECT_EQ(d->frames, 3);
  EXPECT_EQ(d->mean_db, 0.0);
  EXPECT_EQ(d->over_2db, 0.0);
  EXPECT_EQ(d->over_4db, 0.0);
}

TEST(EnvelopeDistortionTest, KnownEnvelopesGiveTheirDistortion) {
  // Envelopes 10 x 0.5 and 10 x 0.2 dB, scaled by the window's middle
  // value: about 3 dB apart in every frame.
  FakeAnalysis analysis(10.0);
  const StatusOr<EnvelopeDistortion> d =
      MeasureEnvelopeDistortion(Signals({true, true}, 0.5, 0.2), analysis.lpc);
  ASSERT_TRUE(d.IsOk());
  EXPECT_NEAR(d->mean_db, 3.0, 1e-3);
  EXPECT_EQ(d->over_2db, 100.0);
  EXPECT_EQ(d->over_4db, 0.0);
}

TEST(EnvelopeDistortionTest, ComparesShapesWithTheGainLeftOut) {
  FakeAnalysis analysis(10.0);
  EXPECT_CALL(analysis.lpc,
              EnvelopeDb(AllOf(Field(&LpcFit::gain, 1.0),
                               Field(&LpcFit::a, SizeIs(kEnvelopeOrder))),
                         kEnvelopeFft))
      .Times(2);
  ASSERT_TRUE(MeasureEnvelopeDistortion(Signals({true}, 0.5, 0.2), analysis.lpc)
                  .IsOk());
}

TEST(EnvelopeDistortionTest, FramesOutsideTheMaskAreNotCounted) {
  FakeAnalysis analysis(10.0);
  PreparedSignals p = Signals({true, false, true, false}, 0.5, 0.2);
  // Frame 1 would be a large outlier, if it were counted.
  for (int i = kEnvelopeFrame; i < 2 * kEnvelopeFrame; ++i) {
    p.output[static_cast<std::size_t>(i)] = -5.0;
  }
  const StatusOr<EnvelopeDistortion> d =
      MeasureEnvelopeDistortion(p, analysis.lpc);
  ASSERT_TRUE(d.IsOk());
  EXPECT_EQ(d->frames, 2);
  EXPECT_EQ(d->over_4db, 0.0);
}

TEST(EnvelopeDistortionTest, SilentOutputIsComparedWithAFlatShape) {
  // The original's envelope is 10 x 0.5, about 5 dB, against the flat 0 dB
  // of a silent output.
  FakeAnalysis analysis(10.0);
  const StatusOr<EnvelopeDistortion> d =
      MeasureEnvelopeDistortion(Signals({true}, 0.5, 0.0), analysis.lpc);
  ASSERT_TRUE(d.IsOk());
  EXPECT_EQ(d->frames, 1);
  EXPECT_NEAR(d->mean_db, 5.0, 1e-3);
  EXPECT_EQ(d->over_4db, 100.0);
}

TEST(EnvelopeDistortionTest, SilentOriginalFramesAreNotCounted) {
  FakeAnalysis analysis(10.0);
  const StatusOr<EnvelopeDistortion> d =
      MeasureEnvelopeDistortion(Signals({true, true}, 0.0, 0.5), analysis.lpc);
  ASSERT_TRUE(d.IsOk());
  EXPECT_EQ(d->frames, 0);
  EXPECT_EQ(d->mean_db, 0.0);
}

TEST(EnvelopeDistortionTest, AnalysisFailureIsReturned) {
  testing::MockLpcAnalysis lpc;
  EXPECT_CALL(lpc, Analyse(_, _)).WillOnce(Return(InternalError("broken")));
  EXPECT_EQ(MeasureEnvelopeDistortion(Signals({true}, 0.5, 0.2), lpc)
                .GetStatus()
                .Code(),
            StatusCode::kInternal);
}

}  // namespace
}  // namespace speak5220::core::quality
