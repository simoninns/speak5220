/*
 * File:        encoder_golden.h
 * Module:      test support
 * Purpose:     Reads the encoder's reference data in app/golden/encoder/
 *              and compares encodings with it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_ENCODER_GOLDEN_H_
#define SPEAK5220_TESTS_SUPPORT_ENCODER_GOLDEN_H_

#include <QJsonArray>
#include <QJsonObject>
#include <cstddef>
#include <filesystem>
#include <map>
#include <set>
#include <span>
#include <string>
#include <vector>

#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/util/status.h"

namespace speak5220::testing {

// The encoder's reference data, app/golden/encoder/.
std::filesystem::path EncoderGoldenDir();

// One corpus file's reference encoding for one option set.
struct ReferenceEncoding {
  std::string name;
  std::vector<core::lpc::Frame> frames;
  // Frames whose coding the reference changes under a disturbance at the
  // level of floating-point noise (app/scripts/golden/encoder.py).
  std::set<std::size_t> near_ties;
};

// The reference encoder's defaults, tms5220enc.py DEFAULTS, written out:
// five of them differ from EncoderOptions{}, and the options the reference
// does not have are off (resonance_hold_db unset, voicing_min_run 1;
// bass_cut_hz and treble_tilt are off by default). The encoder always quietens
// pauses, which the reference does not: compare its frames with
// ReferenceFrames().
core::encoder::EncoderOptions ReferenceEncoderOptions();

// A reference encoding's frames as the encoder gives them for the same
// options: with its pauses quietened (encoder::QuietPauses()), which keeps
// the frame count, so near-tie frames keep their indices.
std::vector<core::lpc::Frame> ReferenceFrames(
    std::span<const core::lpc::Frame> reference);

// The option sets of option_sets.tsv, by name, as EncoderOptions over
// ReferenceEncoderOptions().
core::StatusOr<std::map<std::string, core::encoder::EncoderOptions>>
ReadOptionSets(const std::filesystem::path& path);

// The encodings of frames/SET.tsv, by corpus file name.
core::StatusOr<std::map<std::string, ReferenceEncoding>> ReadReferenceEncodings(
    const std::filesystem::path& path);

// A JSON file's top-level object.
core::StatusOr<QJsonObject> ReadJsonObject(const std::filesystem::path& path);

// A JSON array of numbers as doubles, and of integers as ints.
std::vector<double> JsonDoubles(const QJsonArray& array);
std::vector<int> JsonInts(const QJsonArray& array);

// A WAV file's samples as doubles (-1..1), with its rate.
struct WavSamples {
  int sample_rate = 0;
  std::vector<double> samples;
};
core::StatusOr<WavSamples> ReadWavSamples(const std::filesystem::path& path);

// How an encoding compares with its reference (plan §3.2).
struct EncodingComparison {
  bool identical = false;
  // Every differing frame is a near-tie frame of the reference.
  bool confined_to_near_ties = false;
  // The frames that differ (a missing frame counts as different).
  std::vector<std::size_t> differing;
};
EncodingComparison CompareEncoding(const ReferenceEncoding& reference,
                                   const std::vector<core::lpc::Frame>& frames);

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_ENCODER_GOLDEN_H_
