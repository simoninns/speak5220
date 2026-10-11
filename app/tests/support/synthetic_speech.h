/*
 * File:        synthetic_speech.h
 * Module:      test support
 * Purpose:     A deterministic speech-like test signal (voiced bursts, a
 *              fricative and pauses) for unit tests of the quality measures
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_SYNTHETIC_SPEECH_H_
#define SPEAK5220_TESTS_SUPPORT_SYNTHETIC_SPEECH_H_

#include <vector>

namespace speak5220::testing {

// About two seconds at 8 kHz, peak about 0.5: a 0.2 s pause, a vowel
// (harmonics of 110-140 Hz shaped by three formants) for 0.6 s, a pause,
// a 0.25 s fricative (differenced noise), a second vowel at 200 Hz for
// 0.5 s, and a closing pause. The same samples on every platform.
std::vector<double> SyntheticSpeech();

// The fundamental of the second vowel, and where it lies in the signal.
inline constexpr double kSecondVowelHz = 200.0;
inline constexpr int kSecondVowelStart = 9600;
inline constexpr int kSecondVowelEnd = 13600;

// `x` delayed by `delay` samples (zeros shifted in), same length.
std::vector<double> Delayed(const std::vector<double>& x, int delay);

// Deterministic white noise of RMS `rms`, `n` samples.
std::vector<double> WhiteNoise(int n, double rms);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_SYNTHETIC_SPEECH_H_
