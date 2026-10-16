/*
 * File:        project.cpp
 * Module:      project
 * Purpose:     A project: its settings, its phrases in PHROM order, its
 *              export presets, and the editing operations on them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <cstddef>
#include <map>
#include <numeric>
#include <optional>
#include <ranges>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/packaging/acorn_phrom.h"
#include "core/project/phrase.h"
#include "core/project/phrase_name.h"
#include "core/project/phrom_allocator.h"
#include "core/util/logging.h"

namespace speak5220::core::project {
namespace {

constexpr char kFirstAsciiKey = 32;
constexpr char kLastAsciiKey = 126;

// True for well-formed UTF-8 (no overlong forms, surrogates or values past
// U+10FFFF).
bool IsValidUtf8(std::string_view text) {
  constexpr unsigned kContinuationMask = 0xC0;
  constexpr unsigned kContinuation = 0x80;
  std::size_t i = 0;
  while (i < text.size()) {
    const auto lead = static_cast<unsigned char>(text[i]);
    std::size_t length = 0;
    unsigned minimum = 0;
    unsigned value = 0;
    if (lead < 0x80) {
      ++i;
      continue;
    }
    if ((lead & 0xE0U) == 0xC0) {
      length = 2;
      minimum = 0x80;
      value = lead & 0x1FU;
    } else if ((lead & 0xF0U) == 0xE0) {
      length = 3;
      minimum = 0x800;
      value = lead & 0x0FU;
    } else if ((lead & 0xF8U) == 0xF0) {
      length = 4;
      minimum = 0x10000;
      value = lead & 0x07U;
    } else {
      return false;
    }
    if (i + length > text.size()) {
      return false;
    }
    for (std::size_t j = 1; j < length; ++j) {
      const auto next = static_cast<unsigned char>(text[i + j]);
      if ((next & kContinuationMask) != kContinuation) {
        return false;
      }
      value = (value << 6U) | (next & 0x3FU);
    }
    constexpr unsigned kSurrogateFirst = 0xD800;
    constexpr unsigned kSurrogateLast = 0xDFFF;
    constexpr unsigned kLastCodePoint = 0x10FFFF;
    if (value < minimum || value > kLastCodePoint ||
        (value >= kSurrogateFirst && value <= kSurrogateLast)) {
      return false;
    }
    i += length;
  }
  return true;
}

// Checks one phrase on its own: everything but uniqueness and the budget.
// Normalises its name.
Status CheckPhrase(Phrase& phrase, const ProjectSettings& settings) {
  if (!IsValidPhraseId(phrase.id)) {
    return InvalidArgumentError(fmt::format(
        "\"{}\" is not a phrase id (a lower-case UUID)", phrase.id));
  }
  StatusOr<std::string> name = NormalisePhraseName(phrase.name);
  if (!name.IsOk()) {
    return name.GetStatus();
  }
  phrase.name = *std::move(name);
  if (!IsValidUtf8(phrase.description)) {
    return InvalidArgumentError(
        fmt::format("the description of {} is not UTF-8", phrase.name));
  }
  if (phrase.source.has_value() == phrase.audio.empty()) {
    return InvalidArgumentError(fmt::format(
        "{} must have both source information and audio, or neither",
        phrase.name));
  }
  if (!phrase.source.has_value() && !phrase.locked) {
    return InvalidArgumentError(fmt::format(
        "{} has no source audio, so it must be locked", phrase.name));
  }
  if (phrase.play_start < 0 ||
      static_cast<std::size_t>(phrase.play_start) > phrase.audio.size()) {
    return OutOfRangeError(
        fmt::format("the play start of {} is outside its audio", phrase.name));
  }
  if (StatusOr<encoder::EncoderOptions> options =
          EffectiveEncoderOptions(settings.default_encoder, phrase.encoder);
      !options.IsOk()) {
    return InvalidArgumentError(
        fmt::format("{}: {}", phrase.name, options.GetStatus().Message()));
  }
  if (phrase.encoding.has_value()) {
    for (const lpc::Frame& frame : phrase.encoding->frames) {
      if (Status status = lpc::ValidateFrame(frame); !status.IsOk()) {
        return InvalidArgumentError(
            fmt::format("{}: {}", phrase.name, status.Message()));
      }
    }
  }
  return OkStatus();
}

std::string_view Plural(int n, std::string_view one, std::string_view many) {
  return n == 1 ? one : many;
}

}  // namespace

Status ValidateProjectSettings(const ProjectSettings& settings) {
  for (const auto& [character, name] : settings.ascii_map) {
    if (character < kFirstAsciiKey || character > kLastAsciiKey) {
      return InvalidArgumentError(
          "an ASCII map key must be a printable ASCII character");
    }
    if (StatusOr<std::string> normal = NormalisePhraseName(name);
        !normal.IsOk() || *normal != name) {
      return InvalidArgumentError(fmt::format(
          "the ASCII map entry for \"{}\" names \"{}\", which is not a valid "
          "phrase name",
          character, name));
    }
  }
  std::set<int> numbers;
  for (const PhromHeader& phrom : settings.phroms) {
    if (phrom.number < 1 || phrom.number > kPhromBudget) {
      return OutOfRangeError(
          fmt::format("PHROM {} is not a PHROM number (1 to {})", phrom.number,
                      kPhromBudget));
    }
    if (!numbers.insert(phrom.number).second) {
      return InvalidArgumentError(fmt::format(
          "PHROM {} has its header strings set twice", phrom.number));
    }
  }
  if (!IsValidUtf8(settings.name)) {
    return InvalidArgumentError("the project name is not UTF-8");
  }
  if (Status status = encoder::ValidateEncoderOptions(settings.default_encoder);
      !status.IsOk()) {
    return InvalidArgumentError("default encoder options: " + status.Message());
  }
  return OkStatus();
}

Status ValidateProjectHeaderStrings(std::span<const std::string> strings) {
  if (Status status = packaging::ValidateHeaderStrings(strings);
      !status.IsOk()) {
    return status;
  }
  std::vector<std::string> longest(strings.begin(), strings.end());
  if (longest.size() > 1) {
    longest[1] += " " + std::to_string(kPhromBudget);
  }
  if (Status status = packaging::ValidateHeaderStrings(longest);
      !status.IsOk()) {
    return InvalidArgumentError(
        fmt::format("{} (with PHROM {}'s number after the second)",
                    status.Message(), kPhromBudget));
  }
  return OkStatus();
}

int AsciiEntriesFor(const ProjectSettings& settings, std::string_view name) {
  return static_cast<int>(std::ranges::count_if(
      settings.ascii_map,
      [name](const auto& entry) { return entry.second == name; }));
}

StatusOr<PhraseCostInput> CostOfPhrase(const Phrase& phrase,
                                       const ProjectSettings& settings) {
  const int ascii_entries = AsciiEntriesFor(settings, phrase.name);
  if (!phrase.encoding.has_value()) {
    return PhraseCostInput{.cost = std::nullopt,
                           .ascii_entries = ascii_entries};
  }
  StatusOr<int> bytes = PackedSize(phrase.encoding->frames);
  if (!bytes.IsOk()) {
    return bytes.GetStatus();
  }
  return PhraseCostInput{
      .cost = PhraseCost(*bytes, static_cast<int>(phrase.name.size())),
      .ascii_entries = ascii_entries};
}

StatusOr<Project> Project::FromContents(ProjectContents contents) {
  if (Status status = ValidateProjectSettings(contents.settings);
      !status.IsOk()) {
    return status;
  }
  std::set<PhraseId> ids;
  std::set<std::string> names;
  for (Phrase& phrase : contents.phrases) {
    if (Status status = CheckPhrase(phrase, contents.settings);
        !status.IsOk()) {
      return status;
    }
    if (!ids.insert(phrase.id).second) {
      return InvalidArgumentError("phrase id used twice: " + phrase.id);
    }
    if (!names.insert(phrase.name).second) {
      return InvalidArgumentError("phrase name used twice: " + phrase.name);
    }
  }
  Project project(std::move(contents));
  if (Status status = project.SetExports(project.contents_.exports).GetStatus();
      !status.IsOk()) {
    return status;
  }
  return project;
}

const Phrase* Project::FindPhrase(const PhraseId& id) const {
  const std::optional<int> index = IndexOf(id);
  return index ? &contents_.phrases[static_cast<std::size_t>(*index)] : nullptr;
}

std::optional<int> Project::IndexOf(const PhraseId& id) const {
  for (std::size_t i = 0; i < contents_.phrases.size(); ++i) {
    if (contents_.phrases[i].id == id) {
      return static_cast<int>(i);
    }
  }
  return std::nullopt;
}

std::set<std::string> Project::PhraseNames() const {
  std::set<std::string> names;
  for (const Phrase& phrase : contents_.phrases) {
    names.insert(phrase.name);
  }
  return names;
}

std::vector<PhraseId> Project::Order() const {
  std::vector<PhraseId> order;
  order.reserve(contents_.phrases.size());
  for (const Phrase& phrase : contents_.phrases) {
    order.push_back(phrase.id);
  }
  return order;
}

Status Project::SetSettings(ProjectSettings settings) {
  if (Status status = ValidateProjectSettings(settings); !status.IsOk()) {
    return status;
  }
  // Every phrase's overrides must still apply to the new defaults.
  for (const Phrase& phrase : contents_.phrases) {
    if (StatusOr<encoder::EncoderOptions> options =
            EffectiveEncoderOptions(settings.default_encoder, phrase.encoder);
        !options.IsOk()) {
      return InvalidArgumentError(
          fmt::format("{}: {}", phrase.name, options.GetStatus().Message()));
    }
  }
  contents_.settings = std::move(settings);
  return OkStatus();
}

StatusOr<std::vector<ExportPreset>> Project::SetExports(
    std::vector<ExportPreset> exports) {
  std::set<std::string> names;
  for (const ExportPreset& preset : exports) {
    if (preset.name.empty()) {
      return InvalidArgumentError("an export preset needs a name");
    }
    if (!names.insert(preset.name).second) {
      return InvalidArgumentError("export preset name used twice: " +
                                  preset.name);
    }
  }
  std::vector<ExportPreset> previous = std::move(contents_.exports);
  contents_.exports = std::move(exports);
  return previous;
}

std::vector<PhraseCostInput> Project::Costs() const {
  std::vector<PhraseCostInput> costs;
  costs.reserve(contents_.phrases.size());
  for (const Phrase& phrase : contents_.phrases) {
    // Frames are validated whenever they enter the project, so packing
    // cannot fail here.
    StatusOr<PhraseCostInput> cost = CostOfPhrase(phrase, contents_.settings);
    costs.push_back(cost.IsOk() ? *cost : PhraseCostInput{});
  }
  return costs;
}

std::vector<PhraseCostInput> Project::CostsInOrder(
    std::span<const int> order) const {
  const std::vector<PhraseCostInput> costs = Costs();
  std::vector<PhraseCostInput> ordered;
  ordered.reserve(order.size());
  for (const int index : order) {
    ordered.push_back(costs[static_cast<std::size_t>(index)]);
  }
  return ordered;
}

PhromAllocation Project::Allocation() const { return AllocatePhroms(Costs()); }

PhraseBudgetCheck::PhraseBudgetCheck(std::vector<PhraseCostInput> costs,
                                     std::size_t index, int name_length,
                                     int limit, std::string name)
    : costs_(std::move(costs)),
      index_(index),
      name_length_(name_length),
      limit_(limit),
      name_(std::move(name)) {}

Status PhraseBudgetCheck::Check(int packed_bytes) const {
  std::vector<PhraseCostInput> costs = costs_;
  costs[index_].cost = PhraseCost(packed_bytes, name_length_);
  const int after = AllocatePhroms(costs).PhromsUsed();
  if (after <= limit_) {
    return OkStatus();
  }
  return ResourceExhaustedError(fmt::format(
      "cannot re-encode {}: the project would need {} PHROMs, and its "
      "budget is {}",
      name_, after, kPhromBudget));
}

StatusOr<PhraseBudgetCheck> Project::BudgetCheckFor(const PhraseId& id) const {
  const std::optional<int> index = IndexOf(id);
  if (!index) {
    return NotFoundError("no phrase with id " + id);
  }
  const Phrase& phrase = contents_.phrases[static_cast<std::size_t>(*index)];
  return PhraseBudgetCheck(Costs(), static_cast<std::size_t>(*index),
                           static_cast<int>(phrase.name.size()),
                           std::max(kPhromBudget, Allocation().PhromsUsed()),
                           phrase.name);
}

Status Project::CheckBudget(std::span<const PhraseCostInput> costs,
                            std::string_view action) const {
  const int now = Allocation().PhromsUsed();
  const int after = AllocatePhroms(costs).PhromsUsed();
  const int limit = std::max(kPhromBudget, now);
  if (after <= limit) {
    return OkStatus();
  }
  Logger(kLogModuleProject)
      ->debug("refused to {}: {} PHROMs needed, limit {}", action, after,
              limit);
  return ResourceExhaustedError(fmt::format(
      "cannot {}: the project would need {} PHROMs, and its budget is {}",
      action, after, kPhromBudget));
}

Status Project::InsertPhrases(std::vector<PlacedPhrase> phrases) {
  std::set<PhraseId> ids;
  std::set<std::string> names = PhraseNames();
  for (const Phrase& phrase : contents_.phrases) {
    ids.insert(phrase.id);
  }
  // Check everything, and work out the costs in the final order, before
  // changing anything.
  std::vector<PhraseCostInput> costs = Costs();
  int previous_index = -1;
  for (PlacedPhrase& placed : phrases) {
    if (placed.index <= previous_index ||
        std::cmp_greater(placed.index, costs.size())) {
      return OutOfRangeError(
          fmt::format("cannot insert a phrase at position {}", placed.index));
    }
    previous_index = placed.index;
    if (Status status = CheckPhrase(placed.phrase, contents_.settings);
        !status.IsOk()) {
      return status;
    }
    if (!ids.insert(placed.phrase.id).second) {
      return InvalidArgumentError("phrase id already used: " +
                                  placed.phrase.id);
    }
    if (!names.insert(placed.phrase.name).second) {
      return InvalidArgumentError(fmt::format(
          "there is already a phrase called {}", placed.phrase.name));
    }
    StatusOr<PhraseCostInput> cost =
        CostOfPhrase(placed.phrase, contents_.settings);
    if (!cost.IsOk()) {
      return cost.GetStatus();
    }
    costs.insert(costs.begin() + placed.index, *cost);
  }
  const int count = static_cast<int>(phrases.size());
  if (Status status = CheckBudget(
          costs,
          fmt::format("add {} {}", count, Plural(count, "phrase", "phrases")));
      !status.IsOk()) {
    return status;
  }
  for (PlacedPhrase& placed : phrases) {
    Logger(kLogModuleProject)
        ->debug("inserted {} at {}", placed.phrase.name, placed.index);
    contents_.phrases.insert(contents_.phrases.begin() + placed.index,
                             std::move(placed.phrase));
  }
  return OkStatus();
}

StatusOr<std::vector<PlacedPhrase>> Project::RemovePhrases(
    std::span<const PhraseId> ids) {
  std::set<int> indices;
  for (const PhraseId& id : ids) {
    const std::optional<int> index = IndexOf(id);
    if (!index) {
      return NotFoundError("no phrase with id " + id);
    }
    if (!indices.insert(*index).second) {
      return InvalidArgumentError("phrase listed twice: " + id);
    }
  }
  std::vector<int> kept;
  for (int i = 0; std::cmp_less(i, contents_.phrases.size()); ++i) {
    if (!indices.contains(i)) {
      kept.push_back(i);
    }
  }
  const int count = static_cast<int>(indices.size());
  if (Status status = CheckBudget(
          CostsInOrder(kept), fmt::format("remove {} {}", count,
                                          Plural(count, "phrase", "phrases")));
      !status.IsOk()) {
    return status;
  }
  std::vector<PlacedPhrase> removed;
  removed.reserve(indices.size());
  for (const int index : indices) {
    removed.push_back(
        {.index = index,
         .phrase =
             std::move(contents_.phrases[static_cast<std::size_t>(index)])});
  }
  // Erase from the back, so the earlier indices stay valid.
  for (const int index : std::views::reverse(indices)) {
    contents_.phrases.erase(contents_.phrases.begin() + index);
  }
  Logger(kLogModuleProject)->debug("removed {} phrase(s)", removed.size());
  return removed;
}

Status Project::ApplyOrder(std::vector<int> order, std::string_view action) {
  if (Status status = CheckBudget(CostsInOrder(order), action);
      !status.IsOk()) {
    return status;
  }
  std::vector<Phrase> reordered;
  reordered.reserve(order.size());
  for (const int index : order) {
    reordered.push_back(
        std::move(contents_.phrases[static_cast<std::size_t>(index)]));
  }
  contents_.phrases = std::move(reordered);
  return OkStatus();
}

StatusOr<std::vector<PhraseId>> Project::MovePhrases(
    std::span<const PhraseId> ids, int destination) {
  const int size = static_cast<int>(contents_.phrases.size());
  if (destination < 0 || destination > size) {
    return OutOfRangeError(
        fmt::format("cannot move phrases to position {}", destination));
  }
  std::set<int> moving;
  std::vector<int> moved;
  for (const PhraseId& id : ids) {
    const std::optional<int> index = IndexOf(id);
    if (!index) {
      return NotFoundError("no phrase with id " + id);
    }
    if (!moving.insert(*index).second) {
      return InvalidArgumentError("phrase listed twice: " + id);
    }
  }
  // The moved phrases keep their relative order, and land where the phrase
  // at `destination` was, counted without them.
  moved.assign(moving.begin(), moving.end());
  std::vector<int> order;
  int insert_at = destination;
  for (int i = 0; i < size; ++i) {
    if (moving.contains(i)) {
      if (i < destination) {
        --insert_at;
      }
    } else {
      order.push_back(i);
    }
  }
  order.insert(order.begin() + insert_at, moved.begin(), moved.end());
  std::vector<PhraseId> previous = Order();
  if (Status status = ApplyOrder(std::move(order), "move the phrases");
      !status.IsOk()) {
    return status;
  }
  return previous;
}

StatusOr<std::vector<PhraseId>> Project::SortPhrases() {
  std::vector<int> order(contents_.phrases.size());
  std::iota(order.begin(), order.end(), 0);
  std::ranges::stable_sort(order, [this](int a, int b) {
    return contents_.phrases[static_cast<std::size_t>(a)].name <
           contents_.phrases[static_cast<std::size_t>(b)].name;
  });
  std::vector<PhraseId> previous = Order();
  if (Status status = ApplyOrder(std::move(order), "sort the phrases");
      !status.IsOk()) {
    return status;
  }
  return previous;
}

StatusOr<std::vector<PhraseId>> Project::SetOrder(
    std::span<const PhraseId> order) {
  if (order.size() != contents_.phrases.size()) {
    return InvalidArgumentError(
        "a new order must list every phrase exactly once");
  }
  std::vector<int> indices;
  std::set<int> seen;
  for (const PhraseId& id : order) {
    const std::optional<int> index = IndexOf(id);
    if (!index || !seen.insert(*index).second) {
      return InvalidArgumentError(
          "a new order must list every phrase exactly once");
    }
    indices.push_back(*index);
  }
  std::vector<PhraseId> previous = Order();
  if (Status status = ApplyOrder(std::move(indices), "reorder the phrases");
      !status.IsOk()) {
    return status;
  }
  return previous;
}

StatusOr<std::string> Project::RenamePhrase(const PhraseId& id,
                                            std::string_view name) {
  const std::optional<int> index = IndexOf(id);
  if (!index) {
    return NotFoundError("no phrase with id " + id);
  }
  StatusOr<std::string> normal = NormalisePhraseName(name);
  if (!normal.IsOk()) {
    return normal.GetStatus();
  }
  Phrase& phrase = contents_.phrases[static_cast<std::size_t>(*index)];
  if (*normal == phrase.name) {
    return phrase.name;
  }
  if (PhraseNames().contains(*normal)) {
    return InvalidArgumentError(
        fmt::format("there is already a phrase called {}", *normal));
  }
  std::vector<PhraseCostInput> costs = Costs();
  PhraseCostInput& cost = costs[static_cast<std::size_t>(*index)];
  if (cost.cost.has_value()) {
    *cost.cost +=
        static_cast<int>(normal->size()) - static_cast<int>(phrase.name.size());
  }
  // The ASCII map names phrases: the renamed phrase has the entries that
  // name its new name.
  cost.ascii_entries = AsciiEntriesFor(contents_.settings, *normal);
  if (Status status = CheckBudget(costs, "rename " + phrase.name);
      !status.IsOk()) {
    return status;
  }
  Logger(kLogModuleProject)->debug("renamed {} to {}", phrase.name, *normal);
  return std::exchange(phrase.name, *std::move(normal));
}

StatusOr<std::string> Project::SetDescription(const PhraseId& id,
                                              std::string description) {
  const std::optional<int> index = IndexOf(id);
  if (!index) {
    return NotFoundError("no phrase with id " + id);
  }
  if (!IsValidUtf8(description)) {
    return InvalidArgumentError("a description must be UTF-8 text");
  }
  return std::exchange(
      contents_.phrases[static_cast<std::size_t>(*index)].description,
      std::move(description));
}

StatusOr<int> Project::SetPlayStart(const PhraseId& id, int sample) {
  const std::optional<int> index = IndexOf(id);
  if (!index) {
    return NotFoundError("no phrase with id " + id);
  }
  Phrase& phrase = contents_.phrases[static_cast<std::size_t>(*index)];
  if (sample < 0 || static_cast<std::size_t>(sample) > phrase.audio.size()) {
    return OutOfRangeError(
        fmt::format("the play start of {} must be from 0 to {}, not {}",
                    phrase.name, phrase.audio.size(), sample));
  }
  return std::exchange(phrase.play_start, sample);
}

StatusOr<EncodingChange> Project::SetEncoding(const PhraseId& id,
                                              EncodingChange change) {
  const std::optional<int> index = IndexOf(id);
  if (!index) {
    return NotFoundError("no phrase with id " + id);
  }
  Phrase& phrase = contents_.phrases[static_cast<std::size_t>(*index)];
  if (phrase.locked) {
    return FailedPreconditionError(fmt::format(
        "{} is locked: its frames are never re-encoded", phrase.name));
  }
  Phrase updated = phrase;
  updated.encoder = change.overrides;
  updated.encoding = change.encoding;
  if (Status status = CheckPhrase(updated, contents_.settings);
      !status.IsOk()) {
    return status;
  }
  std::vector<PhraseCostInput> costs = Costs();
  StatusOr<PhraseCostInput> cost = CostOfPhrase(updated, contents_.settings);
  if (!cost.IsOk()) {
    return cost.GetStatus();
  }
  costs[static_cast<std::size_t>(*index)] = *cost;
  if (Status status = CheckBudget(costs, "re-encode " + phrase.name);
      !status.IsOk()) {
    return status;
  }
  EncodingChange previous{.overrides = std::move(phrase.encoder),
                          .encoding = std::move(phrase.encoding)};
  phrase.encoder = std::move(change.overrides);
  phrase.encoding = std::move(change.encoding);
  return previous;
}

StatusOr<encoder::EncoderOptions> Project::SetDefaultEncoder(
    encoder::EncoderOptions options) {
  encoder::EncoderOptions previous = contents_.settings.default_encoder;
  ProjectSettings settings = contents_.settings;
  settings.default_encoder = options;
  if (Status status = SetSettings(std::move(settings)); !status.IsOk()) {
    return status;
  }
  Logger(kLogModuleProject)->debug("changed the default encoder options");
  return previous;
}

StatusOr<PhromSettings> Project::SetPhromSettings(PhromSettings settings) {
  ProjectSettings changed = contents_.settings;
  PhromSettings previous{.ascii_map = changed.ascii_map,
                         .phroms = changed.phroms};
  changed.ascii_map = std::move(settings.ascii_map);
  changed.phroms = std::move(settings.phroms);
  if (Status status = ValidateProjectSettings(changed); !status.IsOk()) {
    return status;
  }
  for (const PhromHeader& phrom : changed.phroms) {
    if (Status status = packaging::ValidateHeaderStrings(phrom.header_strings);
        !status.IsOk()) {
      return InvalidArgumentError(
          fmt::format("PHROM {}: {}", phrom.number, status.Message()));
    }
  }
  std::vector<PhraseCostInput> costs = Costs();
  for (std::size_t i = 0; i < costs.size(); ++i) {
    costs[i].ascii_entries =
        AsciiEntriesFor(changed, contents_.phrases[i].name);
  }
  if (Status status = CheckBudget(costs, "change the ASCII map");
      !status.IsOk()) {
    return status;
  }
  contents_.settings = std::move(changed);
  Logger(kLogModuleProject)
      ->debug("changed the ASCII map ({} entries) and PHROM headers ({})",
              contents_.settings.ascii_map.size(),
              contents_.settings.phroms.size());
  return previous;
}

StatusOr<ProjectIdentity> Project::SetIdentity(ProjectIdentity identity) {
  if (!IsValidUtf8(identity.name)) {
    return InvalidArgumentError("the project name is not UTF-8");
  }
  if (Status status = ValidateProjectHeaderStrings(identity.header_strings);
      !status.IsOk()) {
    return status;
  }
  ProjectIdentity previous{
      .name = std::move(contents_.settings.name),
      .header_strings = std::move(contents_.settings.header_strings)};
  contents_.settings.name = std::move(identity.name);
  contents_.settings.header_strings = std::move(identity.header_strings);
  Logger(kLogModuleProject)
      ->debug("changed the project name and header strings");
  return previous;
}

}  // namespace speak5220::core::project
