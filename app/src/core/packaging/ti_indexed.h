/*
 * File:        ti_indexed.h
 * Module:      packaging
 * Purpose:     Builds a 16 KB image in TI's indexed format
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PACKAGING_TI_INDEXED_H_
#define SPEAK5220_CORE_PACKAGING_TI_INDEXED_H_

#include <span>

#include "core/packaging/phrom_image.h"
#include "core/util/status.h"

namespace speak5220::core::packaging {

// TI's indexed layout (speak5220 datasheet §12.8): byte 0 is 0, byte 1 the
// word count, and word i (1..n) has its pointer at byte 2i, low byte
// first. Each word ends with one spare byte after its stop code's byte.
// Stateless; thread-safe.
class TiIndexed {
 public:
  static constexpr int kMaxWords = 255;

  // Builds the image; words are numbered from 1. Fails for more than 255
  // words, or words that do not fit. Re-implements tms5220enc.py
  // tiIndexed(). O(total bytes).
  static StatusOr<PhromImage> Build(std::span<const Word> words);
};

}  // namespace speak5220::core::packaging

#endif  // SPEAK5220_CORE_PACKAGING_TI_INDEXED_H_
