/*
 * File:        coding_tables.h
 * Module:      lpc
 * Purpose:     The TMS5220 coding tables for energy, pitch, K1-K10 and the
 *              chirp
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_LPC_CODING_TABLES_H_
#define SPEAK5220_CORE_LPC_CODING_TABLES_H_

#include <array>
#include <cstddef>
#include <span>

namespace speak5220::core::lpc {

// The synthesis parameters in their parameter-RAM order: energy, pitch,
// then K1 to K10 (speak5220 datasheet §8.9).
inline constexpr int kParameterCount = 12;
inline constexpr int kEnergyParameter = 0;
inline constexpr int kPitchParameter = 1;
inline constexpr int kFirstKParameter = 2;
inline constexpr int kKCount = 10;

// speak5220 datasheet §8.1, Table 8-4: energy per index. Index 15 is the
// stop code, never multiplied: it sets the target to 0.
inline constexpr std::array<int, 16> kEnergyTable = {
    0, 1, 2, 3, 4, 6, 8, 11, 16, 23, 33, 47, 63, 85, 114, 0};

// speak5220 datasheet §8.1, Table 8-5: the value held in the pitch register
// per index; the period is the value plus one sample (§8.6).
inline constexpr std::array<int, 64> kPitchTable = {
    0,   15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25, 26,
    27,  28,  29,  30,  31,  32,  33,  34,  35,  36,  37,  38, 39,
    40,  41,  42,  44,  46,  48,  50,  52,  53,  56,  58,  60, 62,
    65,  68,  70,  72,  76,  78,  80,  84,  86,  91,  94,  98, 101,
    105, 109, 114, 118, 122, 127, 132, 137, 142, 148, 153, 159};

// speak5220 datasheet §8.1, Table 8-6: the reflection coefficients, as
// 10-bit values scaled by 512.
inline constexpr std::array<int, 32> kK1Table = {
    -501, -498, -497, -495, -493, -491, -488, -482, -478, -474, -469,
    -464, -459, -452, -445, -437, -412, -380, -339, -288, -227, -158,
    -81,  -1,   80,   157,  226,  287,  337,  379,  411,  436};
inline constexpr std::array<int, 32> kK2Table = {
    -328, -303, -274, -244, -211, -175, -138, -99, -59, -18, 24,
    64,   105,  143,  180,  215,  248,  278,  306, 331, 354, 374,
    392,  408,  422,  435,  445,  455,  463,  470, 476, 506};
inline constexpr std::array<int, 16> kK3Table = {
    -441, -387, -333, -279, -225, -171, -117, -63,
    -9,   45,   98,   152,  206,  260,  314,  368};
inline constexpr std::array<int, 16> kK4Table = {
    -328, -273, -217, -161, -106, -50, 5,   61,
    116,  172,  228,  283,  339,  394, 450, 506};
inline constexpr std::array<int, 16> kK5Table = {
    -328, -282, -235, -189, -142, -96, -50, -3,
    43,   90,   136,  182,  229,  275, 322, 368};
inline constexpr std::array<int, 16> kK6Table = {
    -256, -212, -168, -123, -79, -35, 10,  54,
    98,   143,  187,  232,  276, 320, 365, 409};
inline constexpr std::array<int, 16> kK7Table = {
    -308, -260, -212, -164, -117, -69, -21, 27,
    75,   122,  170,  218,  266,  314, 361, 409};
inline constexpr std::array<int, 8> kK8Table = {-256, -161, -66, 29,
                                                124,  219,  314, 409};
inline constexpr std::array<int, 8> kK9Table = {-256, -176, -96, -15,
                                                65,   146,  226, 307};
inline constexpr std::array<int, 8> kK10Table = {-205, -132, -59, 14,
                                                 87,   160,  234, 307};

// speak5220 datasheet §8.1: the width in bits of K1 to K10.
inline constexpr std::array<int, kKCount> kKBits = {5, 5, 4, 4, 4,
                                                    4, 4, 3, 3, 3};

// speak5220 datasheet §8.1, Table 8-7: the chirp ROM, 52 locations;
// locations 21 to 51 hold 0, and a counter above 51 reads location 51.
inline constexpr std::array<int, 52> kChirpTable = {
    0,  3,  15, 40, 76, 108, 113, 80, 37, 38, 76, 68, 26, 50, 59, 19, 55, 26,
    37, 31, 29, 0,  0,  0,   0,   0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,   0,   0,  0,  0,  0,  0,  0,  0,  0,  0};

// The coding table of K1 (k = 0) to K10 (k = 9).
constexpr std::span<const int> KTable(int k) {
  switch (k) {
    case 0:
      return kK1Table;
    case 1:
      return kK2Table;
    case 2:
      return kK3Table;
    case 3:
      return kK4Table;
    case 4:
      return kK5Table;
    case 5:
      return kK6Table;
    case 6:
      return kK7Table;
    case 7:
      return kK8Table;
    case 8:
      return kK9Table;
    default:
      return kK10Table;
  }
}

// The decoded value of a parameter (kEnergyParameter, kPitchParameter, or
// kFirstKParameter + k) for a coded index.
constexpr int ParameterValue(int parameter, int index) {
  const auto at = static_cast<std::size_t>(index);
  if (parameter == kEnergyParameter) {
    return kEnergyTable.at(at);
  }
  if (parameter == kPitchParameter) {
    return kPitchTable.at(at);
  }
  return KTable(parameter - kFirstKParameter)[at];
}

}  // namespace speak5220::core::lpc

#endif  // SPEAK5220_CORE_LPC_CODING_TABLES_H_
