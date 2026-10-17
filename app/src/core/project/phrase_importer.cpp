/*
 * File:        phrase_importer.cpp
 * Module:      project
 * Purpose:     Turns a WAV file into a new phrase: read, mixed, resampled
 *              to 8 kHz, optionally normalised, on the 24-bit grid
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrase_importer.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <cmath>
#include <functional>
#include <set>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/pcm24.h"
#include "core/project/phrase_name.h"
#include "core/util/logging.h"

namespace speak5220::core::project {
namespace {

float Peak(std::span<const float> samples) {
  float peak = 0.0F;
  for (const float sample : samples) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

}  // namespace

PhraseImporter::PhraseImporter(audio::IWavReader& reader,
                               dsp::IResampler& resampler, IFileDigest& digest)
    : reader_(&reader), resampler_(&resampler), digest_(&digest) {}

StatusOr<ImportResult> PhraseImporter::Import(
    const ImportRequest& request, const PhraseId& id,
    const std::set<std::string>& taken) {
  const auto log = Logger(kLogModuleProject);
  if (!IsValidPhraseId(id)) {
    return InvalidArgumentError("not a phrase id: " + id);
  }
  ImportResult result;
  Phrase& phrase = result.phrase;
  phrase.id = id;
  if (request.name.has_value()) {
    StatusOr<std::string> name = NormalisePhraseName(*request.name);
    if (!name.IsOk()) {
      return name.GetStatus();
    }
    if (taken.contains(*name)) {
      return InvalidArgumentError(
          fmt::format("there is already a phrase called {}", *name));
    }
    phrase.name = *std::move(name);
  } else {
    phrase.name = SuggestPhraseName(request.path.stem().string(), taken);
  }

  // Refused from the header, before a long file's samples are read.
  StatusOr<audio::WavInfo> info = reader_->ReadInfo(request.path);
  if (!info.IsOk()) {
    return info.GetStatus();
  }
  if (info->Seconds() > kMaxImportSeconds) {
    return OutOfRangeError(fmt::format(
        "{} is {:.1f} s long; a phrase can be at most {:.0f} s, so that it "
        "fits on one 16 KB PHROM. Split the recording into shorter phrases",
        request.path.string(), info->Seconds(), kMaxImportSeconds));
  }

  StatusOr<audio::DecodedWav> wav = reader_->Read(request.path);
  if (!wav.IsOk()) {
    return wav.GetStatus();
  }
  StatusOr<std::string> sha256 = digest_->Sha256(request.path);
  if (!sha256.IsOk()) {
    return sha256.GetStatus();
  }
  phrase.source =
      SourceInfo{.file = request.path.filename().string(),
                 .rate = wav->sample_rate,
                 .channels = wav->channels,
                 .duration = static_cast<double>(wav->samples.size()) /
                             static_cast<double>(wav->sample_rate),
                 .sha256 = *std::move(sha256),
                 .unknown = {}};

  // Resample, or copy 8 kHz input unchanged.
  if (wav->sample_rate == kProjectSampleRate) {
    phrase.audio = std::move(wav->samples);
  } else {
    StatusOr<std::vector<float>> resampled = resampler_->Resample(
        wav->samples, wav->sample_rate, kProjectSampleRate);
    if (!resampled.IsOk()) {
      return resampled.GetStatus();
    }
    phrase.audio = *std::move(resampled);
  }
  if (phrase.audio.empty()) {
    return DataLossError(request.path.string() + " holds no audio");
  }

  // Step 3: normalise only clipping or very quiet input, and only if asked.
  const float peak = Peak(phrase.audio);
  result.clipped = peak >= kClippingPeak;
  result.quiet = peak < kQuietPeak;
  if (request.normalise && (result.clipped || result.quiet) && peak > 0.0F) {
    const float gain = kNormalisedPeak / peak;
    for (float& sample : phrase.audio) {
      sample *= gain;
    }
    result.normalised = true;
  }

  // Step 4: the 24-bit grid the project stores (samples beyond full scale
  // are clipped).
  audio::QuantiseToPcm24(phrase.audio);
  log->info("imported {} as {}: {} Hz, {} channel(s), {:.2f} s{}{}",
            phrase.source->file, phrase.name, phrase.source->rate,
            phrase.source->channels, phrase.source->duration,
            result.clipped ? ", clipping" : "", result.quiet ? ", quiet" : "");
  if (result.normalised) {
    log->info("normalised {} to a peak of {}", phrase.name, kNormalisedPeak);
  }
  return result;
}

void PhraseImporter::ImportInBackground(
    ITaskRunner& runner, ImportRequest request, PhraseId id,
    std::set<std::string> taken,
    std::function<void(StatusOr<ImportResult>)> done) {
  runner.Post([this, request = std::move(request), id = std::move(id),
               taken = std::move(taken),
               done = std::move(done)]() { done(Import(request, id, taken)); });
}

}  // namespace speak5220::core::project
