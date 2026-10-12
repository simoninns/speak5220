/*
 * File:        quality_measures.cpp
 * Module:      quality
 * Purpose:     The quality measures of an encoding together: the signals
 *              prepared, then the spectral envelope, pitch and voicing, and
 *              intelligibility
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/quality_measures.h"

#include <algorithm>
#include <cassert>
#include <span>

#include "core/quality/encoded_audio.h"
#include "core/quality/estoi.h"

namespace speak5220::core::quality {

StatusOr<QualityMeasurement> MeasurePrepared(const PreparedSignals& prepared,
                                             ILpcAnalysis& lpc,
                                             IPitchEstimator& pitch) {
  QualityMeasurement m;
  m.speech_frames = static_cast<int>(std::ranges::count(prepared.speech, true));
  if (m.speech_frames == 0) {
    return FailedPreconditionError("the source holds no speech to measure");
  }
  StatusOr<EnvelopeDistortion> envelope =
      MeasureEnvelopeDistortion(prepared, lpc);
  if (!envelope.IsOk()) {
    return envelope.GetStatus();
  }
  m.envelope = *envelope;
  StatusOr<PitchErrors> errors = MeasurePitchErrors(prepared, pitch);
  if (!errors.IsOk()) {
    return errors.GetStatus();
  }
  m.pitch = *errors;
  StatusOr<double> estoi = MeasureIntelligibility(prepared);
  if (estoi.IsOk()) {
    m.estoi = *estoi;
  } else if (estoi.GetStatus().Code() != StatusCode::kFailedPrecondition) {
    return estoi.GetStatus();
  }
  return m;
}

StatusOr<QualityMeasurement> MeasureQuality(std::span<const double> source,
                                            std::span<const lpc::Frame> frames,
                                            const QualityAnalysers& analysers) {
  assert(analysers.synth != nullptr && analysers.vad != nullptr &&
         analysers.lpc != nullptr && analysers.pitch != nullptr);
  StatusOr<EncodedAudio> audio =
      RenderEncodedAudio(*analysers.synth, source, frames);
  if (!audio.IsOk()) {
    return audio.GetStatus();
  }
  StatusOr<PreparedSignals> prepared =
      PrepareSignals(source, *audio, *analysers.vad);
  if (!prepared.IsOk()) {
    return prepared.GetStatus();
  }
  return MeasurePrepared(*prepared, *analysers.lpc, *analysers.pitch);
}

}  // namespace speak5220::core::quality
