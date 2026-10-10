/*
 * File:        filter_constants.h
 * Module:      dsp
 * Purpose:     The encoder's fixed Butterworth filters at 8 kHz, as
 *              second-order sections with their filtfilt constants
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_DSP_FILTER_CONSTANTS_H_
#define SPEAK5220_CORE_DSP_FILTER_CONSTANTS_H_

#include "core/dsp/iir_filter.h"

namespace speak5220::core::dsp {

// Written by app/scripts/generate_filter_constants.py from SciPy 1.17.1:
// scipy.signal.butter(..., output="sos"), sosfilt_zi() and the
// padding length of sosfiltfilt(). Do not edit by hand.

// Order-2 Butterworth highpass at 60 Hz; tms5220enc.py
// _butter("highpass", 60.0, 2): removes rumble before analysis.
inline constexpr SosFilter<1> kHighPass60Hz = {
    .sections = {{
        {.b = {0.9672272827147773, -1.9344545654295546, 0.9672272827147773},
         .a = {-1.9333802258799302, 0.9355289049791784},
         .zi = {-0.9672272827147914, 0.9672272827147905}},
    }},
    .pad_length = 9,
};

// Order-4 Butterworth lowpass at 1000 Hz; tms5220enc.py
// _butter("lowpass", 1000.0): the band the pitch tracker uses.
inline constexpr SosFilter<2> kLowPass1kHz = {
    .sections = {{
        {.b = {0.010209480791203138, 0.020418961582406275,
               0.010209480791203138},
         .a = {-0.8553979327751704, 0.20971535775655478},
         .zi = {0.10504853443894861, -0.013961895107098575}},
        {.b = {1.0, 2.0, 1.0},
         .a = {-1.1130298541633479, 0.5740619150839545},
         .zi = {0.8847419847698489, -0.4588038998538032}},
    }},
    .pad_length = 15,
};

}  // namespace speak5220::core::dsp

#endif  // SPEAK5220_CORE_DSP_FILTER_CONSTANTS_H_
