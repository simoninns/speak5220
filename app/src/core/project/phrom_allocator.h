/*
 * File:        phrom_allocator.h
 * Module:      project
 * Purpose:     Assigns a project's phrases to 16 KB PHROMs in list order,
 *              within the 64-PHROM project budget
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PHROM_ALLOCATOR_H_
#define SPEAK5220_CORE_PROJECT_PHROM_ALLOCATOR_H_

#include <optional>
#include <span>
#include <vector>

#include "core/packaging/phrom_image.h"

namespace speak5220::core::project {

// Plan §4.1, decision D4: the project budget, 64 PHROMs (1 MB). A limit
// chosen for testing, not a hardware one.
inline constexpr int kPhromBudget = 64;

// speak5220 datasheet §12.8: what every Acorn PHROM image spends before
// its words. The 64-byte header, the 95 ASCII pointers (word numbers
// 32-126, bytes 64-253), the 0xFF after the last pointer, and the one-byte
// null word the unused ASCII entries point to. A PHROM whose every ASCII
// entry names one of its words, as Acorn's PHROM A does, has no null word
// (kPhromNullWordBytes), so one byte more for words.
inline constexpr int kPhromHeaderBytes = 64;
inline constexpr int kPhromAsciiEntries = 95;
inline constexpr int kPhromAsciiPointerBytes = kPhromAsciiEntries * 2;
inline constexpr int kPhromNullWordBytes = 1;
inline constexpr int kPhromFixedOverhead = kPhromHeaderBytes +
                                           kPhromAsciiPointerBytes + 1 +
                                           kPhromNullWordBytes;  // 256

// The bytes left for words in one PHROM with a null word; one more without.
inline constexpr int kPhromWordCapacity =
    static_cast<int>(packaging::kPhromSize) - kPhromFixedOverhead;  // 16128

// Plan §4.1: each word's 2-byte pointer.
inline constexpr int kPhromPointerBytes = 2;

// What a phrase costs in its PHROM (plan §4.1): its packed bytes, its name
// (one byte per character, stored before the data) and its pointer.
constexpr int PhraseCost(int packed_bytes, int name_length) {
  return packed_bytes + name_length + kPhromPointerBytes;
}

// One phrase, as the allocator sees it: its cost, or nothing when it has
// not been encoded yet (it then takes no space), and how many of the
// project's ASCII entries name it.
struct PhraseCostInput {
  std::optional<int> cost;
  int ascii_entries = 0;
};

// What happened to a phrase.
enum class Placement {
  kPlaced,      // in `phrom`
  kTooLarge,    // bigger than an empty PHROM: in no PHROM (plan §4.1)
  kNotEncoded,  // no frames yet: in no PHROM
};

// A phrase's place.
struct PhraseSlot {
  Placement placement = Placement::kNotEncoded;
  // The PHROM, from 1, and the word number within it (from 127,
  // AcornPhrom::kFirstWordNumber), when placed.
  int phrom = 0;
  int word_number = 0;
  // Its cost, when encoded.
  int cost = 0;

  friend bool operator==(const PhraseSlot& a, const PhraseSlot& b) = default;
};

// One PHROM's contents.
struct PhromFill {
  // The indices of its phrases, in list order.
  std::vector<int> phrases;
  // The bytes its words use, the fixed overhead included (less the null
  // word once its phrases have every ASCII entry).
  int used_bytes = kPhromFixedOverhead;
  // The ASCII entries its phrases have.
  int ascii_entries = 0;

  int FreeBytes() const {
    return static_cast<int>(packaging::kPhromSize) - used_bytes;
  }

  friend bool operator==(const PhromFill& a, const PhromFill& b) = default;
};

// The assignment of a whole project.
struct PhromAllocation {
  // One per phrase, in list order.
  std::vector<PhraseSlot> phrases;
  // One per PHROM in use. None when no phrase is placed.
  std::vector<PhromFill> phroms;

  int PhromsUsed() const { return static_cast<int>(phroms.size()); }
  bool WithinBudget() const { return PhromsUsed() <= kPhromBudget; }
  // The bytes of every PHROM in use, overheads included: the project's
  // size.
  int TotalBytes() const;
  // The free bytes in the last PHROM, or 0 when none is in use.
  int FreeInLastPhrom() const;
};

// Plan §4.1: fills PHROM 1 in list order and starts the next PHROM when the
// next phrase does not fit; earlier PHROMs are never back-filled, so the
// result is predictable. A PHROM whose phrases have all 95 ASCII entries
// needs no null word, so takes one byte more (AcornPhrom::Build() leaves it
// out). A phrase too big for an empty PHROM is placed nowhere and flagged; one
// not yet encoded is skipped. PHROMs past the budget are still counted, so a
// caller can see by how much a change would exceed it. Pure; O(phrases).
PhromAllocation AllocatePhroms(std::span<const PhraseCostInput> phrases);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PHROM_ALLOCATOR_H_
