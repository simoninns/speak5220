/*
 * File:        phrase_importer.h
 * Module:      project
 * Purpose:     Turns a WAV file into a new phrase: read, mixed, resampled
 *              to 8 kHz, optionally normalised, on the 24-bit grid
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PHRASE_IMPORTER_H_
#define SPEAK5220_CORE_PROJECT_PHRASE_IMPORTER_H_

#include <filesystem>
#include <functional>
#include <optional>
#include <set>
#include <string>

#include "core/audio/wav_io.h"
#include "core/dsp/resampler.h"
#include "core/project/phrase.h"
#include "core/util/file_digest.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"

namespace speak5220::core::project {

// Plan §3.1: input whose peak reaches this (about -0.01 dBFS) is taken to
// clip, and input whose peak stays below kQuietPeak (-30 dBFS) is very
// quiet. Either may be normalised, if the user agrees, to kNormalisedPeak
// (-1 dBFS); otherwise the level is left alone, since the encoder sets its
// own (target_rms).
inline constexpr float kClippingPeak = 0.999F;
inline constexpr float kQuietPeak = 0.0316F;
inline constexpr float kNormalisedPeak = 0.891F;

// What to import.
struct ImportRequest {
  std::filesystem::path path = {};
  // The phrase's name; by default one is made from the file name
  // (SuggestPhraseName()).
  std::optional<std::string> name = std::nullopt;
  // Normalise clipping or very quiet input (plan §3.1: the user is asked
  // first). Input that is neither is never changed.
  bool normalise = false;
};

// A new phrase, not yet in a project, and what was found on the way.
struct ImportResult {
  Phrase phrase;
  bool clipped = false;
  bool quiet = false;
  bool normalised = false;
};

// Plan §3.1 over injected readers, so each step can be tested with mocks:
// read the WAV (any rate and channel count, mixed to mono), resample to
// 8 kHz with the best sinc converter (or copy, if already 8 kHz), check
// for clipping and quiet input, normalise if asked, and move the samples
// to the 24-bit grid the project stores. The source's name, rate,
// channels, duration and SHA-256 go into the phrase. The original file is
// not kept (decision D1). Thread-safe if its collaborators are; they must
// outlive it.
class PhraseImporter {
 public:
  PhraseImporter(audio::IWavReader& reader, dsp::IResampler& resampler,
                 IFileDigest& digest);

  // Makes a phrase with `id` from the request. `taken` holds the names
  // already in the project: a requested name must not be one of them, and
  // a made-up name avoids them. Fails if the file cannot be read or
  // resampled, or the name is invalid or taken.
  StatusOr<ImportResult> Import(const ImportRequest& request,
                                const PhraseId& id,
                                const std::set<std::string>& taken);

  // Import() as a background job on `runner`. `done` is called with the
  // result on the thread that ran the job.
  void ImportInBackground(ITaskRunner& runner, ImportRequest request,
                          PhraseId id, std::set<std::string> taken,
                          std::function<void(StatusOr<ImportResult>)> done);

 private:
  audio::IWavReader* reader_;
  dsp::IResampler* resampler_;
  IFileDigest* digest_;
};

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PHRASE_IMPORTER_H_
