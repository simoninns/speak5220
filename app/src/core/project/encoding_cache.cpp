/*
 * File:        encoding_cache.cpp
 * Module:      project
 * Purpose:     Keeps phrases' cached encodings current: the hash of their
 *              inputs, and re-encoding within the project budget
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/encoding_cache.h"

#include <spdlog/fmt/fmt.h>

#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/audio/pcm24.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/util/logging.h"
#include "core/util/sha256.h"

namespace speak5220::core::project {
namespace {

// Bytes of one 24-bit code in the hash, little-endian.
constexpr int kCodeBytes = 3;

}  // namespace

EncodingCache::EncodingCache(encoder::IEncoder& encoder) : encoder_(&encoder) {}

std::string EncodingCache::SettingsHash(std::span<const float> audio,
                                        const encoder::EncoderOptions& options,
                                        std::string_view encoder_version) {
  Sha256 hash;
  hash.Add("speak5220 encoding\nencoder ");
  hash.Add(encoder_version);
  hash.Add("\n");
  for (const encoder::EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    hash.Add(info.field);
    hash.Add("=");
    hash.Add(encoder::EncoderOptionText(options, info.field));
    hash.Add("\n");
  }
  hash.Add(fmt::format("audio {} {}\n", kProjectSampleRate, audio.size()));
  std::vector<std::uint8_t> codes;
  codes.reserve(audio.size() * kCodeBytes);
  for (const float sample : audio) {
    const auto code = static_cast<std::uint32_t>(audio::ToPcm24(sample));
    for (int i = 0; i < kCodeBytes; ++i) {
      codes.push_back(
          static_cast<std::uint8_t>(code >> (8U * static_cast<unsigned>(i))));
    }
  }
  hash.Add(codes);
  return hash.Hex();
}

StatusOr<bool> EncodingCache::IsCurrent(
    const Phrase& phrase, const encoder::EncoderOptions& defaults) {
  if (phrase.locked) {
    return true;
  }
  StatusOr<encoder::EncoderOptions> options =
      EffectiveEncoderOptions(defaults, phrase.encoder);
  if (!options.IsOk()) {
    return options.GetStatus();
  }
  if (!phrase.encoding.has_value()) {
    return false;
  }
  return phrase.encoding->encoder_version == encoder::kEncoderVersion &&
         phrase.encoding->settings_hash ==
             SettingsHash(phrase.audio, *options, encoder::kEncoderVersion);
}

StatusOr<CachedEncoding> EncodingCache::Encode(
    const Phrase& phrase, const encoder::EncoderOptions& defaults,
    const EncoderOverrides& overrides) {
  if (phrase.audio.empty()) {
    return FailedPreconditionError(
        fmt::format("{} has no source audio to encode", phrase.name));
  }
  StatusOr<encoder::EncoderOptions> options =
      EffectiveEncoderOptions(defaults, overrides);
  if (!options.IsOk()) {
    return options.GetStatus();
  }
  const std::vector<double> samples(phrase.audio.begin(), phrase.audio.end());
  StatusOr<encoder::EncodeResult> result =
      encoder_->Encode(samples, kProjectSampleRate, *options);
  if (!result.IsOk()) {
    return Status(result.GetStatus().Code(),
                  fmt::format("cannot encode {}: {}", phrase.name,
                              result.GetStatus().Message()));
  }
  return MakeEncoding(phrase.audio, *options, std::move(result->frames));
}

CachedEncoding EncodingCache::MakeEncoding(
    std::span<const float> audio, const encoder::EncoderOptions& options,
    std::vector<lpc::Frame> frames) {
  CachedEncoding encoding;
  encoding.encoder_version = std::string(encoder::kEncoderVersion);
  encoding.settings_hash =
      SettingsHash(audio, options, encoder::kEncoderVersion);
  encoding.frames = std::move(frames);
  return encoding;
}

StatusOr<EncodingChange> EncodingCache::ChangeSettings(
    Project& project, const PhraseId& id, EncoderOverrides overrides) {
  const Phrase* phrase = project.FindPhrase(id);
  if (phrase == nullptr) {
    return NotFoundError("no phrase with id " + id);
  }
  if (phrase->locked) {
    return FailedPreconditionError(fmt::format(
        "{} is locked: its frames are never re-encoded", phrase->name));
  }
  StatusOr<CachedEncoding> encoding =
      Encode(*phrase, project.Settings().default_encoder, overrides);
  if (!encoding.IsOk()) {
    return encoding.GetStatus();
  }
  const std::string name = phrase->name;
  StatusOr<EncodingChange> previous = project.SetEncoding(
      id,
      {.overrides = std::move(overrides), .encoding = *std::move(encoding)});
  if (!previous.IsOk() &&
      previous.GetStatus().Code() == StatusCode::kResourceExhausted) {
    return ResourceExhaustedError(fmt::format(
        "the new settings for {} were not applied: {}. It keeps its previous "
        "settings and encoding.",
        name, previous.GetStatus().Message()));
  }
  return previous;
}

RefreshReport EncodingCache::Refresh(Project& project, bool force) {
  RefreshReport report;
  const auto log = Logger(kLogModuleProject);
  for (const PhraseId& id : project.Order()) {
    const Phrase& phrase = *project.FindPhrase(id);
    if (phrase.locked || phrase.audio.empty()) {
      ++report.skipped;
      continue;
    }
    if (!force) {
      StatusOr<bool> current =
          IsCurrent(phrase, project.Settings().default_encoder);
      if (!current.IsOk()) {
        report.failures.push_back({.id = id, .status = current.GetStatus()});
        continue;
      }
      if (*current) {
        ++report.skipped;
        continue;
      }
    }
    StatusOr<CachedEncoding> encoding =
        Encode(phrase, project.Settings().default_encoder, phrase.encoder);
    if (!encoding.IsOk()) {
      report.failures.push_back({.id = id, .status = encoding.GetStatus()});
      continue;
    }
    const std::string name = phrase.name;
    StatusOr<EncodingChange> stored = project.SetEncoding(
        id, {.overrides = phrase.encoder, .encoding = *std::move(encoding)});
    if (!stored.IsOk()) {
      log->warn("{}: {}", name, stored.GetStatus().Message());
      report.failures.push_back({.id = id, .status = stored.GetStatus()});
      continue;
    }
    log->debug("encoded {}", name);
    ++report.encoded;
  }
  log->info("encoded {} phrase(s), {} left alone, {} failed", report.encoded,
            report.skipped, report.failures.size());
  return report;
}

}  // namespace speak5220::core::project
