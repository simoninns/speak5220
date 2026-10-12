/*
 * File:        quality_measures.h
 * Module:      quality
 * Purpose:     The quality measures of an encoding together: the signals
 *              prepared, then the spectral envelope, pitch and voicing, and
 *              intelligibility
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_QUALITY_MEASURES_H_
#define SPEAK5220_CORE_QUALITY_QUALITY_MEASURES_H_

#include <optional>
#include <span>

#include "core/lpc/frame.h"
#include "core/quality/envelope_distortion.h"
#include "core/quality/lpc_envelope.h"
#include "core/quality/pitch_errors.h"
#include "core/quality/pitch_estimator.h"
#include "core/quality/signal_preparation.h"
#include "core/quality/voice_activity.h"
#include "core/synth/synth.h"
#include "core/util/status.h"

namespace speak5220::core::quality {

// The primary measures of one encoding (plan §3.3).
struct QualityMeasurement {
  EnvelopeDistortion envelope;
  PitchErrors pitch;
  // ESTOI, absent when the phrase has too little sound for it (about
  // 0.4 s within the mask).
  std::optional<double> estoi;
  // The frames the voice activity mask counts.
  int speech_frames = 0;
};

// The analysers the measures use, which must outlive the calls.
struct QualityAnalysers {
  synth::ISynth* synth = nullptr;
  IVoiceActivity* vad = nullptr;
  ILpcAnalysis* lpc = nullptr;
  IPitchEstimator* pitch = nullptr;
};

// The measures of already prepared signals. Fails if an analyser does.
StatusOr<QualityMeasurement> MeasurePrepared(const PreparedSignals& prepared,
                                             ILpcAnalysis& lpc,
                                             IPitchEstimator& pitch);

// The measures of `frames` (ending in a stop) against `source` (8 kHz,
// -1..1, as given to the encoder): spoken through the synth's VSM path,
// placed against the source (RenderEncodedAudio()), prepared
// (PrepareSignals()) and measured. Fails if the frames cannot be spoken,
// the mask holds no speech, or an analyser fails. About 0.1 s per second
// of speech; not on the GUI thread. Thread-safe if the analysers are.
StatusOr<QualityMeasurement> MeasureQuality(std::span<const double> source,
                                            std::span<const lpc::Frame> frames,
                                            const QualityAnalysers& analysers);

}  // namespace speak5220::core::quality

#endif  // SPEAK5220_CORE_QUALITY_QUALITY_MEASURES_H_
