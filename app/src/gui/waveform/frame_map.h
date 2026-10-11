/*
 * File:        frame_map.h
 * Module:      waveform
 * Purpose:     Which frame the chip is speaking at each sample of the
 *              encoded audio, and how the encoded pane describes it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_GUI_WAVEFORM_FRAME_MAP_H_
#define SPEAK5220_GUI_WAVEFORM_FRAME_MAP_H_

#include <QString>
#include <cstdint>
#include <optional>

#include "core/lpc/frame.h"

namespace speak5220::app {

// The first encoded sample of frame `frame`'s stretch: frames last 200
// samples (speak5220 datasheet §8.5), and frame j's values hold from
// sample 125 + 200 j, where they are half-way reached, to 325 + 200 j,
// half-way to the next frame's (encoder::kReachedSample). Encoded samples
// count from the sample period in which the first frame is read.
std::int64_t FrameStretchStart(int frame);

// The frame whose stretch holds encoded sample `sample`, among
// `frame_count` frames; none before the first stretch or after the last.
std::optional<int> FrameAtEncodedSample(std::int64_t sample, int frame_count);

// The hover text for frame `index`: its number, kind and indices, for
// example "Frame 12: voiced, energy 9, pitch 31, K 20 15 9 8 7 6 5 4 3 2".
QString DescribeFrame(int index, const core::lpc::Frame& frame);

}  // namespace speak5220::app

#endif  // SPEAK5220_GUI_WAVEFORM_FRAME_MAP_H_
