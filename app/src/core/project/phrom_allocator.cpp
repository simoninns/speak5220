/*
 * File:        phrom_allocator.cpp
 * Module:      project
 * Purpose:     Assigns a project's phrases to 16 KB PHROMs in list order,
 *              within the 64-PHROM project budget
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrom_allocator.h"

#include <optional>
#include <span>

#include "core/packaging/acorn_phrom.h"

namespace speak5220::core::project {

int PhromAllocation::TotalBytes() const {
  int total = 0;
  for (const PhromFill& phrom : phroms) {
    total += phrom.used_bytes;
  }
  return total;
}

int PhromAllocation::FreeInLastPhrom() const {
  return phroms.empty() ? 0 : phroms.back().FreeBytes();
}

namespace {

// The bytes `phrom` would use with a phrase of `cost` and `ascii_entries`
// added.
int UsedWith(const PhromFill& phrom, int cost, int ascii_entries) {
  const bool had_all = phrom.ascii_entries >= kPhromAsciiEntries;
  const bool has_all =
      phrom.ascii_entries + ascii_entries >= kPhromAsciiEntries;
  return phrom.used_bytes + cost -
         (has_all && !had_all ? kPhromNullWordBytes : 0);
}

}  // namespace

PhromAllocation AllocatePhroms(std::span<const PhraseCostInput> phrases) {
  PhromAllocation allocation;
  allocation.phrases.resize(phrases.size());
  for (std::size_t i = 0; i < phrases.size(); ++i) {
    PhraseSlot& slot = allocation.phrases[i];
    const std::optional<int>& cost = phrases[i].cost;
    if (!cost.has_value()) {
      continue;  // kNotEncoded
    }
    slot.cost = *cost;
    const int ascii_entries = phrases[i].ascii_entries;
    constexpr int kSize = static_cast<int>(packaging::kPhromSize);
    if (UsedWith(PhromFill{}, slot.cost, ascii_entries) > kSize) {
      slot.placement = Placement::kTooLarge;
      continue;
    }
    if (allocation.phroms.empty() ||
        UsedWith(allocation.phroms.back(), slot.cost, ascii_entries) > kSize) {
      allocation.phroms.emplace_back();
    }
    PhromFill& phrom = allocation.phroms.back();
    phrom.used_bytes = UsedWith(phrom, slot.cost, ascii_entries) - slot.cost;
    phrom.ascii_entries += ascii_entries;
    slot.placement = Placement::kPlaced;
    slot.phrom = allocation.PhromsUsed();
    slot.word_number = packaging::AcornPhrom::kFirstWordNumber +
                       static_cast<int>(phrom.phrases.size());
    phrom.phrases.push_back(static_cast<int>(i));
    phrom.used_bytes += slot.cost;
  }
  return allocation;
}

}  // namespace speak5220::core::project
