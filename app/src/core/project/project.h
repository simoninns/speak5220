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

// An export the Export dialog remembers and the CLI can run (plan §4).
struct ExportPreset {
  std::string name = {};
  // "acorn-phrom", ... (plan §6).
  std::string format = {};
  // "all" phrases.
  std::string scope = "all";
  // For PHROM images: "separate" (one file per PHROM) or "concatenated".
  std::string layout = {};
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
  // The options every phrase starts from; unknown keys of that mapping.
  encoder::EncoderOptions default_encoder;
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
  StatusOr<EncodingChange> SetEncoding(const PhraseId& id,
                                       EncodingChange change) override;

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
