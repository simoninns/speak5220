/*
 * File:        phrase.h
 * Module:      project
 * Purpose:     A phrase of a project: its name, source audio, encoder
 *              settings and cached encoding
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PHRASE_H_
#define SPEAK5220_CORE_PROJECT_PHRASE_H_

#include <cstdint>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// A phrase's identity: a UUID in its canonical lower-case form, which also
// names its directory in the project (phrases/<id>/, plan §4).
using PhraseId = std::string;

// The rate of a project's audio (plan §3.1).
inline constexpr int kProjectSampleRate = 8000;

// YAML keys this version does not know, in the order read, each with its
// value as YAML text. They are written back unchanged, so a project saved
// by an older version keeps what a newer one added (plan §4).
using UnknownKeys = std::vector<std::pair<std::string, std::string>>;

// The encoder options a phrase sets for itself, over the project's
// defaults: field name (EncoderOptionInfo::field) to text, as
// SetEncoderOption() reads it. Only the options given here differ from the
// defaults; the rest follow the project's defaults when those change.
using EncoderOverrides = std::map<std::string, std::string>;

// Files of a phrase's directory, or of the project, that this version
// does not use, by path; kept and saved again unchanged.
using ExtraFiles = std::map<std::string, std::vector<std::uint8_t>>;

// Where a phrase's audio came from (plan §3.1). A plain value type.
struct SourceInfo {
  // The imported file's name, without its directory.
  std::string file = {};
  // Its sample rate and channel count, before resampling and mixing.
  int rate = 0;
  int channels = 0;
  // Its length in seconds.
  double duration = 0.0;
  // The SHA-256 of the file's bytes, as 64 lower-case hex digits.
  std::string sha256 = {};
  UnknownKeys unknown = {};

  friend bool operator==(const SourceInfo& a, const SourceInfo& b) = default;
};

// A phrase's frames and the inputs they were made from (plan §3.4). For an
// unlocked phrase they are a cache, redone when the hash of the inputs no
// longer matches; a locked phrase's frames are its content. A plain value
// type.
struct CachedEncoding {
  // encoder::kEncoderVersion when the frames were made.
  std::string encoder_version = {};
  // EncodingCache::SettingsHash() of the audio, the options and the
  // encoder version. Empty for frames that were not encoded here.
  std::string settings_hash = {};
  // The frames, ending in a stop.
  std::vector<lpc::Frame> frames = {};
  // Quality measured with the encoding (MOS-LQO), and the phrase's anchor
  // score (plan §3.3), when measured.
  std::optional<double> pesq = std::nullopt;
  std::optional<double> anchor = std::nullopt;
  // Unknown keys of frames.yaml and of its quality mapping.
  UnknownKeys unknown = {};
  UnknownKeys quality_unknown = {};

  friend bool operator==(const CachedEncoding& a,
                         const CachedEncoding& b) = default;
};

// One phrase. A plain value type; Project keeps the rules (names unique
// and valid, ids unique) between phrases.
struct Phrase {
  PhraseId id;
  // Valid by NormalisePhraseName().
  std::string name;
  // Free UTF-8 text, kept only in the project.
  std::string description;
  // Absent only for a locked phrase made from frames alone, such as a word
  // imported from a PHROM image.
  std::optional<SourceInfo> source;
  // The source at kProjectSampleRate, mono, on the 24-bit grid
  // (audio::QuantiseToPcm24()). Empty when there is no source.
  std::vector<float> audio;
  // Options set for this phrase alone, and unknown keys of its encoder
  // mapping.
  EncoderOverrides encoder;
  UnknownKeys encoder_unknown;
  // A locked phrase is never re-encoded (plan §3.4).
  bool locked = false;
  // Where playback starts, in samples of `audio`.
  int play_start = 0;
  // The frames; absent until the phrase is first encoded.
  std::optional<CachedEncoding> encoding;
  UnknownKeys unknown;
  // Other files of phrases/<id>/, by name within that directory.
  ExtraFiles extra_files;

  friend bool operator==(const Phrase& a, const Phrase& b) = default;
};

// True for a UUID in canonical form: 8-4-4-4-12 lower-case hex digits.
// Only such ids are accepted, since they become paths in the project.
bool IsValidPhraseId(std::string_view id);

// A new random UUID in canonical form. Thread-safe.
PhraseId NewPhraseId();

// The options a phrase is encoded with: `defaults` with `overrides`
// applied. Fails if an override names no option or holds a value the
// option does not accept.
StatusOr<encoder::EncoderOptions> EffectiveEncoderOptions(
    const encoder::EncoderOptions& defaults, const EncoderOverrides& overrides);

// The overrides that give `options` over `defaults`: the text of each
// option whose text differs (EncoderOptionText()), so applying them to
// `defaults` with EffectiveEncoderOptions() gives `options` back. Pure.
EncoderOverrides OverridesFor(const encoder::EncoderOptions& defaults,
                              const encoder::EncoderOptions& options);

// The bytes the frames pack to, up to and including the stop (one is added
// if there is none): a phrase's data in a PHROM. Fails for an invalid
// frame.
StatusOr<int> PackedSize(std::span<const lpc::Frame> frames);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PHRASE_H_
