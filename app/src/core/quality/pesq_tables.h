/*
 * File:        pesq_tables.h
 * Module:      quality
 * Purpose:     The constants of ITU-T P.862 narrowband PESQ at 8 kHz: the
 *              Bark bands, hearing thresholds, filter curves and scaling
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_QUALITY_PESQ_TABLES_H_
#define SPEAK5220_CORE_QUALITY_PESQ_TABLES_H_

#include <array>

// The Recommendation's text describes each stage but leaves most of its
// numbers to the ANSI-C reference implementation, which Annex A makes an
// integral part of P.862 and gives precedence over the text. These values
// are taken from it (plan decision D6), for 8 kHz narrowband operation
// only (Amendment 2, P.862 mode).

namespace speak5220::core::quality::pesq_tables {

// The only sample rate supported: narrowband telephony.
inline constexpr int kSampleRate = 8000;

// ITU-T P.862 §10.1.3.1: the envelope frames for voice activity and crude
// alignment are 4 ms, 32 samples.
inline constexpr int kDownsample = 32;

// ITU-T P.862 §10.1.3.2: fine alignment uses 64 ms Hann frames.
inline constexpr int kAlignFftSize = 512;

// ITU-T P.862 §10.2.1.1: the perceptual model's 32 ms Hann frames, 50%
// overlapped.
inline constexpr int kFrameSize = 256;

// Silence added before and after each signal, in envelope frames: room
// for delays of up to 300 ms.
inline constexpr int kSearchBuffer = 75;
inline constexpr int kSearchBufferSamples = kSearchBuffer * kDownsample;

// Zeros appended after each signal: 320 ms.
inline constexpr int kDataPaddingSamples = 320 * (kSampleRate / 1000);

// Voice activity: an utterance is at least this many envelope frames.
inline constexpr int kMinSpeechLength = 4;
inline constexpr int kJoinSpeechLength = 50;
inline constexpr int kMinUtteranceLength = 50;

// At most this many utterances; the last slot is scratch space for the
// utterance-split search.
inline constexpr int kMaxUtterances = 50;

// ITU-T P.862 §10.1.1: both signals are scaled to this mean power over
// the band the level-alignment filter passes.
inline constexpr double kTargetAveragePower = 1e7;

// ITU-T P.862 §10.2.1.3 and §10.2.1.4: the power and loudness scaling
// factors, calibrated so a 1 kHz tone at 40 dB SPL gives a pitch power
// density of 10 000 and a loudness of 1 Sone.
inline constexpr double kPowerScale = 2.764344e-5;
inline constexpr double kLoudnessScale = 1.866055e-1;

// ITU-T P.862 §10.2.8: Zwicker's power above 4 Bark.
inline constexpr double kZwickerPower = 0.23;

// ITU-T P.862 §10.2.16: the PESQ score is 4.5 - kDisturbanceWeight * d -
// kAsymmetryWeight * a.
inline constexpr double kDisturbanceWeight = 0.1;
inline constexpr double kAsymmetryWeight = 0.0309;

// ITU-T P.862 §10.2.11 and §10.2.14-10.2.15: the Lp norms over frequency,
// within split-second intervals and over time, for the symmetric
// disturbance and the asymmetric one.
inline constexpr float kDisturbancePowerFrequency = 2;
inline constexpr float kDisturbancePowerSyllable = 6;
inline constexpr float kDisturbancePowerTime = 2;
inline constexpr float kAsymmetryPowerFrequency = 1;
inline constexpr float kAsymmetryPowerSyllable = 6;
inline constexpr float kAsymmetryPowerTime = 2;

// The number of modified Bark bands at 8 kHz.
inline constexpr int kBands = 42;

// ITU-T P.862 §10.2.5: how many FFT bins each Bark band sums.
inline constexpr std::array<int, kBands> kBinsPerBand = {
    1, 1, 1, 1, 1, 1, 1, 1, 2, 1, 1, 1, 1, 1, 2, 1, 1, 2, 2, 2, 2,
    2, 2, 2, 2, 3, 3, 3, 3, 4, 3, 4, 5, 4, 5, 6, 6, 7, 8, 9, 9, 11};

// Each band's centre, in Bark.
inline constexpr std::array<double, kBands> kBandCentreBark = {
    0.078672,  0.316341,  0.636559,  0.961246,  1.290450,  1.624217,
    1.962597,  2.305636,  2.653383,  3.005889,  3.363201,  3.725371,
    4.092449,  4.464486,  4.841533,  5.223642,  5.610866,  6.003256,
    6.400869,  6.803755,  7.211971,  7.625571,  8.044611,  8.469146,
    8.899232,  9.334927,  9.776288,  10.223374, 10.676242, 11.134952,
    11.599563, 12.070135, 12.546731, 13.029408, 13.518232, 14.013264,
    14.514566, 15.022202, 15.536238, 16.056736, 16.583761, 17.117382};

// ITU-T P.862 §10.2.11: each band's width in Bark, the weights Wf.
inline constexpr std::array<double, kBands> kBandWidthBark = {
    0.157344,
    // NOLINTNEXTLINE(modernize-use-std-numbers): a measured width.
    0.317994, 0.322441, 0.326934, 0.331474, 0.336061, 0.340697, 0.345381,
    0.350114, 0.354897, 0.359729, 0.364611, 0.369544, 0.374529, 0.379565,
    0.384653, 0.389794, 0.394989, 0.400236, 0.405538, 0.410894, 0.416306,
    0.421773, 0.427297, 0.432877, 0.438514, 0.444209, 0.449962, 0.455774,
    0.461645, 0.467577, 0.473569, 0.479621, 0.485736, 0.491912, 0.498151,
    0.504454, 0.510819, 0.517250, 0.523745, 0.530308, 0.536934};

// ITU-T P.862 §10.2.5: the normalisation of each band's summed power.
inline constexpr std::array<double, kBands> kPowerDensityCorrection = {
    100.000000, 99.999992,  100.000000, 100.000008, 100.000008, 100.000015,
    99.999992,  99.999969,  50.000027,  100.000000, 99.999969,  100.000015,
    99.999947,  100.000061, 53.047077,  110.000046, 117.991989, 65.000000,
    68.760147,  69.999931,  71.428818,  75.000038,  76.843384,  80.968781,
    88.646126,  63.864388,  68.155350,  72.547775,  75.584831,  58.379192,
    80.950836,  64.135651,  54.384785,  73.821884,  64.437073,  59.176456,
    65.521278,  61.399822,  58.144047,  57.004543,  64.126297,  59.248363};

// ITU-T P.862 §10.2.1.2: the absolute hearing threshold P0(f) at each
// band's centre, as a pitch power density.
inline constexpr std::array<double, kBands> kAbsoluteThreshold = {
    51286152.000000, 2454709.500000, 70794.593750, 4897.788574, 1174.897705,
    389.045166,      104.712860,     45.708820,    17.782795,   9.772372,
    4.897789,        3.090296,       1.905461,     1.258925,    0.977237,
    0.724436,        0.562341,       0.457088,     0.389045,    0.331131,
    0.295121,        0.269153,       0.257040,     0.251189,    0.251189,
    0.251189,        0.251189,       0.263027,     0.288403,    0.309030,
    0.338844,        0.371535,       0.398107,     0.436516,    0.467735,
    0.489779,        0.501187,       0.501187,     0.512861,    0.524807,
    0.524807,        0.524807};

// A point of a piecewise-linear filter response: frequency (Hz) and gain
// (dB).
struct CurvePoint {
  double hz;
  double db;
};

inline constexpr int kCurvePoints = 26;

// ITU-T P.862 §10.1.1: the filter used only to measure each signal's
// level, passing 350-3250 Hz.
inline constexpr std::array<CurvePoint, kCurvePoints> kLevelAlignCurve = {{
    {.hz = 0., .db = -500},    {.hz = 50., .db = -500},
    {.hz = 100., .db = -500},  {.hz = 125., .db = -500},
    {.hz = 160., .db = -500},  {.hz = 200., .db = -500},
    {.hz = 250., .db = -500},  {.hz = 300., .db = -500},
    {.hz = 350., .db = 0},     {.hz = 400., .db = 0},
    {.hz = 500., .db = 0},     {.hz = 600., .db = 0},
    {.hz = 630., .db = 0},     {.hz = 800., .db = 0},
    {.hz = 1000., .db = 0},    {.hz = 1250., .db = 0},
    {.hz = 1600., .db = 0},    {.hz = 2000., .db = 0},
    {.hz = 2500., .db = 0},    {.hz = 3000., .db = 0},
    {.hz = 3250., .db = 0},    {.hz = 3500., .db = -500},
    {.hz = 4000., .db = -500}, {.hz = 5000., .db = -500},
    {.hz = 6300., .db = -500}, {.hz = 8000., .db = -500},
}};

// ITU-T P.862 §10.1.2: the IRS-like receive characteristic both signals
// are filtered with.
inline constexpr std::array<CurvePoint, kCurvePoints> kIrsReceiveCurve = {{
    {.hz = 0., .db = -200},    {.hz = 50., .db = -40},
    {.hz = 100., .db = -20},   {.hz = 125., .db = -12},
    {.hz = 160., .db = -6},    {.hz = 200., .db = 0},
    {.hz = 250., .db = 4},     {.hz = 300., .db = 6},
    {.hz = 350., .db = 8},     {.hz = 400., .db = 10},
    {.hz = 500., .db = 11},    {.hz = 600., .db = 12},
    {.hz = 700., .db = 12},    {.hz = 800., .db = 12},
    {.hz = 1000., .db = 12},   {.hz = 1300., .db = 12},
    {.hz = 1600., .db = 12},   {.hz = 2000., .db = 12},
    {.hz = 2500., .db = 12},   {.hz = 3000., .db = 12},
    {.hz = 3250., .db = 12},   {.hz = 3500., .db = 4},
    {.hz = 4000., .db = -200}, {.hz = 5000., .db = -200},
    {.hz = 6300., .db = -200}, {.hz = 8000., .db = -200},
}};

// A second-order section b0 b1 b2 a1 a2 (a0 = 1) of the input filter.
struct Biquad {
  float b0;
  float b1;
  float b2;
  float a1;
  float a2;
};

// The input filter applied before voice activity detection and time
// alignment (8 kHz), as eight cascaded sections.
inline constexpr std::array<Biquad, 8> kInputFilter = {{
    {.b0 = 0.885535424F,
     .b1 = -0.885535424F,
     .b2 = 0.000000000F,
     .a1 = -0.771070709F,
     .a2 = 0.000000000F},
    {.b0 = 0.895092588F,
     .b1 = 1.292907193F,
     .b2 = 0.449260174F,
     .a1 = 1.268869037F,
     .a2 = 0.442025372F},
    {.b0 = 4.049527940F,
     .b1 = -7.865190042F,
     .b2 = 3.815662102F,
     .a1 = -1.746859852F,
     .a2 = 0.786305963F},
    {.b0 = 0.500002353F,
     .b1 = -0.500002353F,
     .b2 = 0.000000000F,
     .a1 = 0.000000000F,
     .a2 = 0.000000000F},
    // NOLINTNEXTLINE(modernize-use-std-numbers): a designed coefficient.
    {.b0 = 0.565002834F,
     .b1 = -0.241585934F,
     .b2 = -0.306009671F,
     .a1 = 0.259688659F,
     .a2 = 0.249979657F},
    {.b0 = 2.115237288F,
     .b1 = 0.919935084F,
     .b2 = 1.141240051F,
     .a1 = -1.587313419F,
     .a2 = 0.665935315F},
    {.b0 = 0.912224584F,
     .b1 = -0.224397719F,
     .b2 = -0.641121413F,
     .a1 = -0.246029464F,
     .a2 = -0.556720590F},
    {.b0 = 0.444617727F,
     .b1 = -0.307589321F,
     .b2 = 0.141638062F,
     .a1 = -0.996391149F,
     .a2 = 0.502251622F},
}};

}  // namespace speak5220::core::quality::pesq_tables

#endif  // SPEAK5220_CORE_QUALITY_PESQ_TABLES_H_
