/*
 * File:        encoding_cache.h
 * Module:      project
 * Purpose:     Keeps phrases' cached encodings current: the hash of their
 *              inputs, and re-encoding within the project budget
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_ENCODING_CACHE_H_
#define SPEAK5220_CORE_PROJECT_ENCODING_CACHE_H_

#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// What Refresh() did to one phrase.
struct RefreshFailure {
  PhraseId id;
  Status status;
};

// What Refresh() did.
struct RefreshReport {
  // Phrases encoded and stored.
  int encoded = 0;
  // Phrases already current, locked, or without audio: left alone.
  int skipped = 0;
  // Phrases whose encoding failed or would exceed the budget; they keep
  // their previous encoding.
  std::vector<RefreshFailure> failures;
};

// A phrase's frames are a cache of its source audio and
// encoder settings, invalidated by a hash of (audio, settings, encoder
// version). This class makes and checks those encodings with an injected
// IEncoder, and stores them through Project::SetEncoding(), so a
// re-encoding that would take the project past its budget is not applied.
// Thread-compatible; the encoder must outlive it.
class EncodingCache {
 public:
  explicit EncodingCache(encoder::IEncoder& encoder);

  // The SHA-256 identifying an encoding's inputs: the encoder version,
  // every option's text in EncoderOptionInfos() order (the preprocessing's
  // and the encoder's), and the audio's 24-bit codes. Pure.
  static std::string SettingsHash(std::span<const float> audio,
                                  const encoder::EncoderOptions& options,
                                  std::string_view encoder_version);

  // The cache entry for `frames`, encoded from `audio` with `options` by
  // this encoder version (kEncoderVersion), with no quality measured.
  // Pure.
  static CachedEncoding MakeEncoding(std::span<const float> audio,
                                     const encoder::EncoderOptions& options,
                                     std::vector<lpc::Frame> frames);

  // Whether a phrase's encoding needs no work: it is locked, or its
  // encoding's hash matches its inputs now. A phrase with no encoding is
  // not current. Fails if the phrase's overrides do not apply to
  // `defaults`.
  static StatusOr<bool> IsCurrent(const Phrase& phrase,
                                  const encoder::EncoderOptions& defaults);

  // Encodes a phrase's audio with `overrides` over `defaults`. Fails for a
  // phrase with no audio, invalid options, or an encoder failure.
  StatusOr<CachedEncoding> Encode(const Phrase& phrase,
                                  const encoder::EncoderOptions& defaults,
                                  const EncoderOverrides& overrides);

  // Changes a phrase's encoder overrides and re-encodes it with them. If
  // the new encoding would take the project past its budget, nothing
  // changes and the status says why. Returns the previous
  // overrides and encoding, for undo. Fails also for an unknown or locked
  // phrase.
  StatusOr<EncodingChange> ChangeSettings(Project& project, const PhraseId& id,
                                          EncoderOverrides overrides);

  // Encodes every phrase that is not current, in list order (all unlocked
  // phrases with audio when `force` is set), each stored as it is made, so
  // the budget is checked phrase by phrase. A phrase that fails keeps its
  // previous encoding; the others go on.
  RefreshReport Refresh(Project& project, bool force = false);

 private:
  encoder::IEncoder* encoder_;
};

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_ENCODING_CACHE_H_
