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

#include <cstddef>
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

// Fades loud unvoiced sound before silence and before the end, which
// would otherwise stop audibly (§12.2): an unvoiced frame at energy index
// 6 (value 8) or more is followed by one unvoiced repeat at about a third
// of its energy, in place of the silence frame after it (or appended at
// the end). Quieter unvoiced sound ends as it is, as it does in Acorn's
// PHROM, whose words decay in their own frames (HLD §9). Then applies
// GuardK510() and adds the stop. Departs from tms5220enc.py _addFades(),
// which faded every unvoiced ending in steps of a third down to energy
// value 2.
std::vector<lpc::Frame> AddFades(std::span<const lpc::Frame> frames);

// The pauses QuietPauses() quietens: this many silence frames or more.
// The neutral frame must have a silence frame on each side (below).
inline constexpr int kMinQuietPause = 3;
// Where in the pause the neutral frame goes: after the first silence
// frame, over which the sound before fades with its own K.
inline constexpr std::size_t kQuietPauseAt = 1;

// speak5220 datasheet §12.2, "Silence inside an utterance rings": a
// silence frame keeps the K of the sound before it, and with no
// excitation the lattice settles into a limit cycle a code or two either
// side of zero, which the DAC's -1 to 0 step makes a buzz about 23 dB
// below the speech, for the whole pause. So in a pause of kMinQuietPause
// silence frames or more, the second becomes a voiced frame at energy 1
// with every K at its index nearest zero (NeutralFrame()); the chip then
// holds those K, and the rest of the pause is code 0.
//
// Its place matters. In the first frame of the pause, the K would glide
// to neutral while the sound before fades, which dulls it. After the
// first, the change from silence is inhibited, so the K jump while the
// energy is already 0. With no silence frame after it, the next sound
// would glide in from it instead of starting at once.
//
// A repeat frame after such a pause, which would take the neutral K, is
// sent in full with the K it would have had. A pause is left as it is
// when that cannot be done exactly: a voiced repeat after an unvoiced
// frame, which relies on the unvoiced frame's zeroing of K5-K10. `frames`
// ends in a stop.
std::vector<lpc::Frame> QuietPauses(std::span<const lpc::Frame> frames);

// The frame QuietPauses() puts at the start of a pause: voiced, energy 1,
// `pitch` (a voiced pitch index, 1-63), and every K at its index nearest
// zero.
lpc::Frame NeutralFrame(int pitch);

}  // namespace speak5220::core::encoder

#endif  // SPEAK5220_CORE_ENCODER_FRAME_ENDINGS_H_
