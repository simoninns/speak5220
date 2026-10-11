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
    if (slot.cost > kPhromWordCapacity) {
      slot.placement = Placement::kTooLarge;
      continue;
    }
    if (allocation.phroms.empty() ||
        allocation.phroms.back().used_bytes + slot.cost >
            static_cast<int>(packaging::kPhromSize)) {
      allocation.phroms.emplace_back();
    }
    PhromFill& phrom = allocation.phroms.back();
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
