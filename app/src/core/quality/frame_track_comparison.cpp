/*
 * File:        frame_track_comparison.cpp
 * Module:      quality
 * Purpose:     Compares an encoding's frames with reference frames for the
 *              same speech: voicing, onsets and offsets, energy, filter
 *              shape, pitch, repeats and fade-outs
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/quality/frame_track_comparison.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <numbers>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "core/dsp/lpc_analysis.h"
#include "core/encoder/level_setter.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame.h"
#include "core/synth/reference_synthesizer.h"

namespace speak5220::core::quality {
namespace {

using lpc::FrameType;

// Levels at or below this many codes are too quiet to line up by.
constexpr double kAudibleLevel = 1.0;
// The fewest frames a shift must line up to be considered.
constexpr int kMinimumAlignedFrames = 3;
// The FFT length of the filter responses: kResponseBins = 256 / 2 + 1.
constexpr int kResponseFftLength = 256;
// The floor of a response's magnitude.
constexpr double kResponseFloor = 1e-9;
// K scale of the coding tables (speak5220 datasheet §8.1, Table 8-6).
constexpr double kKScale = 512.0;
constexpr int kSemitonesPerOctave = 12;

bool Sounding(FrameType type) {
  return type == FrameType::kVoiced || type == FrameType::kUnvoiced ||
         type == FrameType::kRepeat;
}

// One frame as it plays: its kind, voicing (none for silence and the
// stop), energy and pitch indices, and the filter of the last full frame.
struct PlayedFrame {
  FrameType type = FrameType::kSilence;
  std::optional<bool> voiced;
  int energy = 0;
  int pitch = 0;
  bool has_filter = false;
  std::array<int, lpc::kKCount> k = {};
  int k_count = 0;
};

std::vector<PlayedFrame> Played(std::span<const lpc::Frame> frames) {
  std::vector<PlayedFrame> out;
  out.reserve(frames.size());
  PlayedFrame state;
  for (const lpc::Frame& frame : frames) {
    state.type = lpc::FrameKind(frame);
    state.energy = frame.energy;
    state.pitch = frame.pitch;
    if (state.type == FrameType::kVoiced ||
        state.type == FrameType::kUnvoiced) {
      state.has_filter = true;
      state.k = frame.k;
      state.k_count = lpc::FrameKCount(frame);
    }
    state.voiced =
        Sounding(state.type) ? std::optional(frame.pitch != 0) : std::nullopt;
    out.push_back(state);
  }
  return out;
}

// The frames before the first stop: those with an output level.
int LevelledFrames(std::span<const lpc::Frame> frames) {
  int n = 0;
  for (const lpc::Frame& frame : frames) {
    if (lpc::FrameKind(frame) == FrameType::kStop) {
      break;
    }
    ++n;
  }
  return n;
}

std::vector<double> FrameLevels(std::span<const lpc::Frame> frames) {
  const synth::ReferenceSynthesis spoken =
      synth::ReferenceSynthesizer::Synthesize(frames);
  return encoder::MeasureOutputLevels(spoken.codes, LevelledFrames(frames))
      .level;
}

// The shift with the least mean |level difference| in dB, the first of
// equals; 0 if no shift lines up kMinimumAlignedFrames audible frames.
int BestShift(std::span<const double> reference,
              std::span<const double> encoded) {
  const auto n_r = static_cast<int>(reference.size());
  const auto n_e = static_cast<int>(encoded.size());
  std::optional<double> best;
  int shift = 0;
  for (int sh = -kMaxTrackShift; sh <= kMaxTrackShift; ++sh) {
    double sum = 0.0;
    int count = 0;
    for (int j = 0; j < n_r; ++j) {
      const int i = j + sh;
      if (i < 0 || i >= n_e) {
        continue;
      }
      const double r = reference[static_cast<std::size_t>(j)];
      const double e = encoded[static_cast<std::size_t>(i)];
      if (r > kAudibleLevel && e > kAudibleLevel) {
        sum += std::fabs(20.0 * std::log10(e / r));
        ++count;
      }
    }
    if (count >= kMinimumAlignedFrames) {
      const double mean = sum / count;
      if (!best.has_value() || mean < *best) {
        best = mean;
        shift = sh;
      }
    }
  }
  return shift;
}

// Whether frame `i` of `played` exists and is voiced; unvoiced, silent or
// absent frames are not.
bool VoicedAt(std::span<const PlayedFrame> played, int i) {
  return i >= 0 && std::cmp_less(i, played.size()) &&
         played[static_cast<std::size_t>(i)].voiced.value_or(false);
}

// The frame lag at which the encoding's own onset (or offset) follows the
// reference's at frame j: the first lag in kFirstBoundaryLag ..
// kLastBoundaryLag, as a bin; the last bin if none.
int BoundaryBin(std::span<const PlayedFrame> encoded, int j, int shift,
                bool onset) {
  const auto n = static_cast<int>(encoded.size());
  for (int d = kFirstBoundaryLag; d <= kLastBoundaryLag; ++d) {
    const int i = j + shift + d;
    const bool found = onset ? (i >= 0 && i < n && VoicedAt(encoded, i) &&
                                (i == 0 || !VoicedAt(encoded, i - 1)))
                             : (i > 0 && i < n && VoicedAt(encoded, i - 1) &&
                                !VoicedAt(encoded, i));
    if (found) {
      return d - kFirstBoundaryLag;
    }
  }
  return kBoundaryLagBins - 1;
}

double ShapeErrorDb(const PlayedFrame& encoded, const PlayedFrame& reference,
                    int order) {
  const std::array<double, kResponseBins> e =
      FilterResponseDb(encoded.k, std::min(order, encoded.k_count));
  const std::array<double, kResponseBins> r =
      FilterResponseDb(reference.k, std::min(order, reference.k_count));
  std::array<double, kResponseBins> d = {};
  double mean = 0.0;
  for (std::size_t b = 0; b < d.size(); ++b) {
    d.at(b) = e.at(b) - r.at(b);
    mean += d.at(b);
  }
  mean /= static_cast<double>(d.size());
  double sum = 0.0;
  for (const double v : d) {
    sum += (v - mean) * (v - mean);
  }
  return std::sqrt(sum / static_cast<double>(d.size()));
}

template <typename T>
void Append(std::vector<T>& to, const std::vector<T>& from) {
  to.insert(to.end(), from.begin(), from.end());
}

void AddFacts(FrameTrackFacts& total, const FrameTrackFacts& add) {
  total.frames += add.frames;
  total.sounding += add.sounding;
  total.repeats += add.repeats;
  for (std::size_t b = 0; b < total.repeat_runs.size(); ++b) {
    total.repeat_runs.at(b) += add.repeat_runs.at(b);
  }
  total.longest_repeat_run =
      std::max(total.longest_repeat_run, add.longest_repeat_run);
  total.fade_outs += add.fade_outs;
  total.fade_frames += add.fade_frames;
  total.bits += add.bits;
}

void AddLags(BoundaryLags& total, const BoundaryLags& add) {
  for (std::size_t b = 0; b < total.size(); ++b) {
    total.at(b) += add.at(b);
  }
}

}  // namespace

std::array<double, kResponseBins> FilterResponseDb(
    std::span<const int> k_indices, int order) {
  std::vector<double> k;
  for (int i = 0; i < order && std::cmp_less(i, k_indices.size()); ++i) {
    k.push_back(lpc::ParameterValue(lpc::kFirstKParameter + i,
                                    k_indices[static_cast<std::size_t>(i)]) /
                kKScale);
  }
  const std::vector<double> a = dsp::StepUp(k);
  std::array<double, kResponseBins> out = {};
  for (int b = 0; b < kResponseBins; ++b) {
    const double w = 2.0 * std::numbers::pi * b / kResponseFftLength;
    std::complex<double> sum = 0.0;
    for (std::size_t n = 0; n < a.size(); ++n) {
      sum += a[n] * std::polar(1.0, -w * static_cast<double>(n));
    }
    const double magnitude = 1.0 / std::abs(sum);
    out.at(static_cast<std::size_t>(b)) =
        20.0 * std::log10(std::max(magnitude, kResponseFloor));
  }
  return out;
}

FrameTrackFacts DescribeFrameTrack(std::span<const lpc::Frame> frames) {
  FrameTrackFacts facts;
  facts.frames = static_cast<int>(frames.size());
  std::vector<FrameType> types;
  types.reserve(frames.size());
  for (const lpc::Frame& frame : frames) {
    types.push_back(lpc::FrameKind(frame));
    facts.bits += lpc::FrameBits(frame);
    facts.sounding += Sounding(types.back()) ? 1 : 0;
  }
  int run = 0;
  for (std::size_t i = 0; i <= types.size(); ++i) {
    if (i < types.size() && types[i] == FrameType::kRepeat) {
      ++facts.repeats;
      ++run;
      continue;
    }
    if (run > 0) {
      ++facts.repeat_runs.at(
          static_cast<std::size_t>(std::min(run, kRunLengthBins) - 1));
      facts.longest_repeat_run = std::max(facts.longest_repeat_run, run);
    }
    run = 0;
  }
  // An unvoiced repeat quieter than the unvoiced frame before it starts a
  // fade; it is a fade-out if the falling run ends the sound.
  const auto unvoiced_repeat = [&](std::size_t i) {
    return types[i] == FrameType::kRepeat && frames[i].pitch == 0;
  };
  std::size_t i = 1;
  while (i < frames.size()) {
    const bool starts =
        unvoiced_repeat(i) &&
        (types[i - 1] == FrameType::kUnvoiced || unvoiced_repeat(i - 1)) &&
        frames[i].energy < frames[i - 1].energy;
    if (!starts) {
      ++i;
      continue;
    }
    std::size_t end = i + 1;
    while (end < frames.size() && unvoiced_repeat(end) &&
           frames[end].energy < frames[end - 1].energy) {
      ++end;
    }
    if (end >= frames.size() || types[end] == FrameType::kSilence ||
        types[end] == FrameType::kStop) {
      ++facts.fade_outs;
      facts.fade_frames += static_cast<int>(end - i);
    }
    i = end;
  }
  return facts;
}

FrameTrackComparison CompareFrameTracks(std::span<const lpc::Frame> reference,
                                        std::span<const lpc::Frame> encoded) {
  FrameTrackComparison out;
  out.reference = DescribeFrameTrack(reference);
  out.encoded = DescribeFrameTrack(encoded);
  const std::vector<double> r_level = FrameLevels(reference);
  const std::vector<double> e_level = FrameLevels(encoded);
  out.shift = BestShift(r_level, e_level);
  const std::vector<PlayedFrame> r = Played(reference);
  const std::vector<PlayedFrame> e = Played(encoded);
  const auto n_r = static_cast<int>(r.size());
  const auto n_e = static_cast<int>(e.size());

  for (int j = 0; j < n_r; ++j) {
    const int i = j + out.shift;
    if (i < 0 || i >= n_e) {
      continue;
    }
    const PlayedFrame& rf = r[static_cast<std::size_t>(j)];
    const PlayedFrame& ef = e[static_cast<std::size_t>(i)];
    if (!rf.voiced.has_value() || !ef.voiced.has_value()) {
      continue;
    }
    if (std::cmp_less(j, r_level.size()) && std::cmp_less(i, e_level.size())) {
      const double rl = r_level[static_cast<std::size_t>(j)];
      const double el = e_level[static_cast<std::size_t>(i)];
      if (rl > kAudibleLevel && el > kAudibleLevel) {
        out.level_differences_db.push_back(20.0 * std::log10(el / rl));
      }
    }
    ++out.matched;
    const bool rv = *rf.voiced;
    const bool ev = *ef.voiced;
    if (rv == ev) {
      ++out.voicing_agrees;
      if (rv) {
        out.pitch_differences.push_back(
            kSemitonesPerOctave *
            std::log2((lpc::kPitchTable.at(static_cast<std::size_t>(ef.pitch)) +
                       1.0) /
                      (lpc::kPitchTable.at(static_cast<std::size_t>(rf.pitch)) +
                       1.0)));
      }
      if (rf.has_filter && ef.has_filter) {
        const double difference = ef.energy - rf.energy;
        if (rv) {
          out.voiced_energy_differences.push_back(difference);
          out.voiced_shape_errors_db.push_back(
              ShapeErrorDb(ef, rf, lpc::kVoicedKCount));
        } else {
          out.unvoiced_energy_differences.push_back(difference);
          out.unvoiced_shape_errors_db.push_back(
              ShapeErrorDb(ef, rf, lpc::kUnvoicedKCount));
        }
      }
    } else if (rv) {
      if (!VoicedAt(r, j - 1)) {
        ++out.voiced_as_unvoiced_onset;
        out.loud_onset_frames += ef.energy >= kLoudOnsetEnergy ? 1 : 0;
      } else if (!VoicedAt(r, j + 1)) {
        ++out.voiced_as_unvoiced_offset;
      } else {
        ++out.voiced_as_unvoiced_inside;
      }
    } else {
      ++out.unvoiced_as_voiced;
    }
  }

  for (int j = 1; j < n_r; ++j) {
    if (VoicedAt(r, j) && !VoicedAt(r, j - 1)) {
      ++out.reference_onsets;
      ++out.onset_lags.at(
          static_cast<std::size_t>(BoundaryBin(e, j, out.shift, true)));
    }
    if (VoicedAt(r, j - 1) && !VoicedAt(r, j)) {
      ++out.reference_offsets;
      ++out.offset_lags.at(
          static_cast<std::size_t>(BoundaryBin(e, j, out.shift, false)));
    }
  }
  return out;
}

void AddComparison(FrameTrackComparison& total,
                   const FrameTrackComparison& add) {
  total.matched += add.matched;
  total.voicing_agrees += add.voicing_agrees;
  total.voiced_as_unvoiced_inside += add.voiced_as_unvoiced_inside;
  total.voiced_as_unvoiced_onset += add.voiced_as_unvoiced_onset;
  total.voiced_as_unvoiced_offset += add.voiced_as_unvoiced_offset;
  total.loud_onset_frames += add.loud_onset_frames;
  total.unvoiced_as_voiced += add.unvoiced_as_voiced;
  total.reference_onsets += add.reference_onsets;
  total.reference_offsets += add.reference_offsets;
  AddLags(total.onset_lags, add.onset_lags);
  AddLags(total.offset_lags, add.offset_lags);
  Append(total.voiced_energy_differences, add.voiced_energy_differences);
  Append(total.unvoiced_energy_differences, add.unvoiced_energy_differences);
  Append(total.voiced_shape_errors_db, add.voiced_shape_errors_db);
  Append(total.unvoiced_shape_errors_db, add.unvoiced_shape_errors_db);
  Append(total.level_differences_db, add.level_differences_db);
  Append(total.pitch_differences, add.pitch_differences);
  AddFacts(total.reference, add.reference);
  AddFacts(total.encoded, add.encoded);
}

}  // namespace speak5220::core::quality
