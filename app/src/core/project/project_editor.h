/*
 * File:        project_editor.h
 * Module:      project
 * Purpose:     The editing operations on a project, each returning what is
 *              needed to reverse it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PROJECT_EDITOR_H_
#define SPEAK5220_CORE_PROJECT_PROJECT_EDITOR_H_

#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/project/phrase.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// Header strings that replace the project's in one PHROM.
struct PhromHeader {
  // The PHROM, from 1.
  int number = 0;
  std::vector<std::string> header_strings = {};
  UnknownKeys unknown = {};

  friend bool operator==(const PhromHeader& a, const PhromHeader& b) = default;
};

// The project settings that shape its PHROMs beyond their words: the
// "ASCII associated" entries and the per-PHROM header strings
// (ProjectSettings::ascii_map, phroms). A plain value type.
struct PhromSettings {
  std::map<char, std::string> ascii_map = {};
  std::vector<PhromHeader> phroms = {};

  friend bool operator==(const PhromSettings& a,
                         const PhromSettings& b) = default;
};

// The project's name and the header strings of its PHROMs
// (ProjectSettings::name, header_strings). A plain value type.
struct ProjectIdentity {
  std::string name = {};
  std::vector<std::string> header_strings = {};

  friend bool operator==(const ProjectIdentity& a,
                         const ProjectIdentity& b) = default;
};

// An export saved in the project under a name, which the CLI runs and the
// Export dialog loads (the "exports" list of project.yaml).
struct ExportPreset {
  std::string name = {};
  // "acorn-phrom", "c-header-phrom", "c-header-speak-external", "wav" or
  // "bbc-dsd".
  std::string format = {};
  // "all" phrases, or the "selected" ones in `phrases`.
  std::string scope = "all";
  // For scope "selected": the phrases' names. They are exported in list
  // order, whatever the order here.
  std::vector<std::string> phrases = {};
  // For PHROM images: "separate" (one file per PHROM) or "concatenated".
  std::string layout = {};
  // For C headers: the SPDX licence identifier of the generated file;
  // empty for GPL-3.0-or-later.
  std::string licence = {};
  // For WAV: "encoded" (the chip's output), "original" (the 8 kHz
  // source) or "preprocessed" (the source as the encoder takes it); empty
  // for "encoded".
  std::string audio = {};
  UnknownKeys unknown = {};

  friend bool operator==(const ExportPreset& a,
                         const ExportPreset& b) = default;
};

// A phrase and its index in the phrase list.
struct PlacedPhrase {
  int index = 0;
  Phrase phrase = {};

  friend bool operator==(const PlacedPhrase& a,
                         const PlacedPhrase& b) = default;
};

// A phrase's encoder settings together with the encoding made from them,
// which change together (a change that would exceed the budget is
// not applied, and the phrase keeps its previous encoding).
struct EncodingChange {
  EncoderOverrides overrides = {};
  std::optional<CachedEncoding> encoding = std::nullopt;

  friend bool operator==(const EncodingChange& a,
                         const EncodingChange& b) = default;
};

// The editing operations shared by the GUI and the CLI. Each one either
// succeeds or fails with nothing changed, and returns what is needed to reverse
// it, so the GUI can wrap each in an undo command and the CLI can call them
// directly. No operation takes the project to more PHROMs than the budget, or
// to more than it already uses if it is over the budget. Implementations are
// thread-compatible.
class IProjectEditor {
 public:
  virtual ~IProjectEditor() = default;

  // Inserts each phrase at its index, taken in ascending order, so each
  // index is a position in the list as it is after the earlier insertions.
  // Names are normalised (NormalisePhraseName()). Fails if the indices are
  // not strictly ascending or out of range, an id is invalid or already
  // used, a name is invalid or already used, a phrase is inconsistent (no
  // source audio but not locked, an invalid frame or override), or the
  // budget would be exceeded. Reverse: RemovePhrases() with their ids.
  virtual Status InsertPhrases(std::vector<PlacedPhrase> phrases) = 0;

  // Removes phrases. Returns them with the indices they had, ascending.
  // Fails for an unknown or repeated id. Reverse: InsertPhrases() with the
  // result.
  virtual StatusOr<std::vector<PlacedPhrase>> RemovePhrases(
      std::span<const PhraseId> ids) = 0;

  // Moves phrases, keeping their relative order, to just before the phrase
  // now at `destination` (0 to the number of phrases; the number of
  // phrases moves them to the end). Returns the previous order. Fails for
  // an unknown or repeated id, a destination out of range, or the budget.
  // Reverse: SetOrder() with the result.
  virtual StatusOr<std::vector<PhraseId>> MovePhrases(
      std::span<const PhraseId> ids, int destination) = 0;

  // Sorts the phrases by name, A to Z (byte order; names that compare
  // equal cannot occur). Returns the previous order. Fails only for the
  // budget. Reverse: SetOrder() with the result.
  virtual StatusOr<std::vector<PhraseId>> SortPhrases() = 0;

  // Puts the phrases in `order`, which must hold every phrase's id once.
  // Returns the previous order. Reverse: SetOrder() with the result.
  virtual StatusOr<std::vector<PhraseId>> SetOrder(
      std::span<const PhraseId> order) = 0;

  // Renames a phrase to `name`, normalised. Returns the previous name.
  // Fails for an unknown id, an invalid name, a name another phrase has,
  // or the budget (a longer name costs more bytes). The ASCII map, which
  // names phrases by name, is not changed. Reverse: RenamePhrase() with
  // the result.
  virtual StatusOr<std::string> RenamePhrase(const PhraseId& id,
                                             std::string_view name) = 0;

  // Sets a phrase's description. Returns the previous one. Fails for an
  // unknown id or text that is not UTF-8. Reverse: SetDescription() with
  // the result.
  virtual StatusOr<std::string> SetDescription(const PhraseId& id,
                                               std::string description) = 0;

  // Sets where a phrase's playback starts, in samples of its audio (0 to
  // the number of samples). Returns the previous value. Fails for an
  // unknown id or a sample outside the audio. Reverse: SetPlayStart() with
  // the result.
  virtual StatusOr<int> SetPlayStart(const PhraseId& id, int sample) = 0;

  // Sets a phrase's encoder overrides and its encoding together. Returns
  // the previous ones. Fails for an unknown id, a locked phrase, an
  // invalid override or frame, or the budget, which leaves the phrase
  // with its previous settings and encoding. Reverse: SetEncoding() with
  // the result.
  virtual StatusOr<EncodingChange> SetEncoding(const PhraseId& id,
                                               EncodingChange change) = 0;
  // Sets the options every phrase starts from (the project's
  // default_encoder). Returns the previous ones. Fails for options out of
  // range, or if a phrase's overrides no longer apply. Encodings made
  // from the old defaults become stale; they are not redone here, so
  // the budget is unaffected. Reverse: SetDefaultEncoder() with the
  // result.
  virtual StatusOr<encoder::EncoderOptions> SetDefaultEncoder(
      encoder::EncoderOptions options) = 0;

  // Replaces the ASCII map and the per-PHROM header strings. Fails,
  // changing nothing, if they are invalid (ValidateProjectSettings(), and
  // header strings that would not fit a PHROM's header,
  // packaging::ValidateHeaderStrings()), or if the new map would take the
  // project past its budget (a PHROM whose phrases have every ASCII entry
  // holds one byte more). Returns the previous ones. Reverse:
  // SetPhromSettings() with the result.
  virtual StatusOr<PhromSettings> SetPhromSettings(PhromSettings settings) = 0;

  // Sets the project's name and the header strings every PHROM starts
  // from. Fails, changing nothing, for a name that is not UTF-8, or header
  // strings that would not fit a PHROM's header with the largest PHROM
  // number appended to the second (ValidateProjectHeaderStrings()). Returns
  // the previous ones. Reverse: SetIdentity() with the result.
  virtual StatusOr<ProjectIdentity> SetIdentity(ProjectIdentity identity) = 0;

  // Replaces the export presets. Fails, changing nothing, for a preset
  // with no name or a name another preset has. Returns the previous ones.
  // Reverse: SetExports() with the result.
  virtual StatusOr<std::vector<ExportPreset>> SetExports(
      std::vector<ExportPreset> exports) = 0;
};

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PROJECT_EDITOR_H_
