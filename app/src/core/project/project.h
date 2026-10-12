/*
 * File:        project.h
 * Module:      project
 * Purpose:     A project: its settings, its phrases in PHROM order, its
 *              export presets, and the editing operations on them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PROJECT_H_
#define SPEAK5220_CORE_PROJECT_PROJECT_H_

#include <cstddef>
#include <map>
#include <optional>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project_editor.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// Header strings that replace the project's in one PHROM (plan §4.1).
struct PhromHeader {
  // The PHROM, from 1.
  int number = 0;
  std::vector<std::string> header_strings = {};
  UnknownKeys unknown = {};

  friend bool operator==(const PhromHeader& a, const PhromHeader& b) = default;
};

// An export the CLI can run (plan §4).
struct ExportPreset {
  std::string name = {};
  // "acorn-phrom", "c-header-phrom", "c-header-speak-external", "wav" or
  // "bbc-dsd" (plan §6).
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

// The project-wide settings (the "project" mapping of project.yaml).
struct ProjectSettings {
  std::string name;
  // The strings of each PHROM's header (speak5220 datasheet §12.8); each
  // PHROM gets its number appended to the second (plan §4.1).
  std::vector<std::string> header_strings = {"(C)2026 speak5220",
                                             "Speak5220 PHROM", "1.00"};
  // Printable ASCII characters (32-126) to phrase names: the "ASCII
  // associated" entries. An entry whose phrase is not in a PHROM is left
  // out of that PHROM.
  std::map<char, std::string> ascii_map;
  // The options every phrase starts from, of both stages; unknown keys of
  // the default_preprocess and default_encoder mappings.
  encoder::EncoderOptions default_encoder;
  UnknownKeys default_preprocess_unknown;
  UnknownKeys default_encoder_unknown;
  // Per-PHROM header strings, by PHROM number.
  std::vector<PhromHeader> phroms;
  UnknownKeys unknown;

  friend bool operator==(const ProjectSettings& a,
                         const ProjectSettings& b) = default;
};

// Everything a project holds, as loaded and saved.
struct ProjectContents {
  ProjectSettings settings;
  // In PHROM order: list order is word order (plan §4.1).
  std::vector<Phrase> phrases;
  std::vector<ExportPreset> exports;
  // Unknown keys of project.yaml's top level, and files outside the
  // phrases' directories that this version does not use.
  UnknownKeys unknown;
  ExtraFiles extra_files;

  friend bool operator==(const ProjectContents& a,
                         const ProjectContents& b) = default;
};

// Checks project settings: printable ASCII keys in the ASCII map with
// valid names, PHROM numbers from 1 to the budget without repeats, and
// valid default encoder options.
Status ValidateProjectSettings(const ProjectSettings& settings);

// Whether one phrase may take a new size, the rest of the project as it is
// (plan §4.1): for the optimiser, which tries many encodings of one
// phrase. A value; Check() is thread-safe.
class PhraseBudgetCheck {
 public:
  // OK if the phrase, at `packed_bytes` (PackedSize()), keeps the project
  // within its budget, or within the PHROMs it fills now if it is over.
  // O(phrases).
  Status Check(int packed_bytes) const;

 private:
  friend class Project;
  PhraseBudgetCheck(std::vector<PhraseCostInput> costs, std::size_t index,
                    int name_length, int limit, std::string name);

  std::vector<PhraseCostInput> costs_;
  std::size_t index_;
  int name_length_;
  int limit_;
  std::string name_;
};

// A project: phrases with unique ids and unique valid names, in PHROM
// order, and the editing operations of IProjectEditor. A value type;
// thread-compatible.
class Project : public IProjectEditor {
 public:
  // An empty project with the default settings.
  Project() = default;

  // A project holding `contents`, which must keep the rules: valid project
  // settings, valid unique ids and names, consistent phrases. The budget is
  // not checked, so an over-budget project still opens.
  static StatusOr<Project> FromContents(ProjectContents contents);

  const ProjectContents& Contents() const { return contents_; }
  const ProjectSettings& Settings() const { return contents_.settings; }
  const std::vector<Phrase>& Phrases() const { return contents_.phrases; }

  // The phrase with `id`, or nullptr.
  const Phrase* FindPhrase(const PhraseId& id) const;
  // Its index in the list.
  std::optional<int> IndexOf(const PhraseId& id) const;
  // Every phrase's name.
  std::set<std::string> PhraseNames() const;
  // Every phrase's id, in list order.
  std::vector<PhraseId> Order() const;

  // Replaces the project settings. Fails, changing nothing, if they are
  // invalid (ValidateProjectSettings()). New default options make
  // encodings stale; they are not redone here.
  Status SetSettings(ProjectSettings settings);
  // Replaces the export presets. Fails for a preset with no name or a
  // repeated name.
  Status SetExports(std::vector<ExportPreset> exports);

  // The PHROMs the phrases fill now (plan §4.1).
  PhromAllocation Allocation() const;
  // The check of a new size for the phrase with `id`. Fails for an
  // unknown id.
  StatusOr<PhraseBudgetCheck> BudgetCheckFor(const PhraseId& id) const;

  // IProjectEditor.
  Status InsertPhrases(std::vector<PlacedPhrase> phrases) override;
  StatusOr<std::vector<PlacedPhrase>> RemovePhrases(
      std::span<const PhraseId> ids) override;
  StatusOr<std::vector<PhraseId>> MovePhrases(std::span<const PhraseId> ids,
                                              int destination) override;
  StatusOr<std::vector<PhraseId>> SortPhrases() override;
  StatusOr<std::vector<PhraseId>> SetOrder(
      std::span<const PhraseId> order) override;
  StatusOr<std::string> RenamePhrase(const PhraseId& id,
                                     std::string_view name) override;
  StatusOr<std::string> SetDescription(const PhraseId& id,
                                       std::string description) override;
  StatusOr<int> SetPlayStart(const PhraseId& id, int sample) override;
  StatusOr<EncodingChange> SetEncoding(const PhraseId& id,
                                       EncodingChange change) override;
  StatusOr<encoder::EncoderOptions> SetDefaultEncoder(
      encoder::EncoderOptions options) override;

  friend bool operator==(const Project& a, const Project& b) {
    return a.contents_ == b.contents_;
  }

 private:
  explicit Project(ProjectContents contents) : contents_(std::move(contents)) {}

  // Fails if the phrases, in this order and with these costs, would fill
  // more PHROMs than the budget allows (or than the project fills now, if
  // that is more).
  Status CheckBudget(std::span<const PhraseCostInput> costs,
                     std::string_view action) const;
  // The costs of the current phrases in list order.
  std::vector<PhraseCostInput> Costs() const;
  // The current phrases' costs in `order` (indices into the list).
  std::vector<PhraseCostInput> CostsInOrder(std::span<const int> order) const;
  // Reorders the list to `order` (indices), after a budget check.
  Status ApplyOrder(std::vector<int> order, std::string_view action);

  ProjectContents contents_;
};

// The cost of a phrase in its PHROM (PhraseCost()), or nothing if it has no
// encoding. Fails for an invalid frame.
StatusOr<PhraseCostInput> CostOfPhrase(const Phrase& phrase);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PROJECT_H_
