/*
 * File:        frame_endings.h
 * Module:      encoder
 * Purpose:     Turns the plan and energies into frames: repeats, fades
 *              before silence, the K5-K10 guard and the stop
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_ENCODER_FRAME_ENDINGS_H_
#define SPEAK5220_CORE_ENCODER_FRAME_ENDINGS_H_

#include <span>
#include <vector>

#include "core/encoder/encoder_types.h"
#include "core/lpc/frame.h"

namespace speak5220::core::encoder {

// Frames from the plan and one energy index per plan entry. A repeat whose
// K differ from those the chip holds (because a frame before it became
// silence) is sent in full. Leading silence is dropped, since it would
// leave the start to the previous utterance (speak5220 datasheet §12.2),
// and so is trailing silence. Re-implements tms5220enc.py _toFrames().
std::vector<lpc::Frame> ToFrames(std::span<const PlanEntry> plan,
                                 std::span<const int> energies);

// No silence before the first full voiced frame (§12.2, "K5-K10 before
// the first voiced frame"): until then the stored K5-K10 indices are the
// previous utterance's, and a silence frame would end their zeroing. Such
// silence becomes an unvoiced repeat at energy 1, which keeps it.
// Re-implements tms5220enc.py _guardK510().
std::vector<lpc::Frame> GuardK510(std::span<const lpc::Frame> frames);

// Fades unvoiced sound before silence and before the end (§12.2), each
// fade frame an unvoiced repeat at about a third of the energy before,
// replacing following silence frames (or appended at the end); then
// applies GuardK510() and adds the stop. With fade_unvoiced false, only
// the guard and the stop. Re-implements tms5220enc.py _addFades().
std::vector<lpc::Frame> AddFades(std::span<const lpc::Frame> frames,
                                 bool fade_unvoiced);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_FRAME_ENDINGS_H_
