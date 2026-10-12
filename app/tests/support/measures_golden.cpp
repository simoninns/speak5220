/*
 * File:        measures_golden.cpp
 * Module:      test support
 * Purpose:     Readers for the quality measures' reference data, and the
 *              subset's encodings prepared as the measures prepare them
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/measures_golden.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/quality/encoded_audio.h"
#include "core/quality/signal_preparation.h"
#include "core/quality/voice_activity.h"
#include "core/synth/synth.h"
#include "core/util/status.h"
#include "support/encoder_golden.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

// A column of numbers; "nan" reads as NaN.
double Number(const std::string& text) {
  return text == "nan" ? std::nan("") : std::stod(text);
}

}  // namespace

std::filesystem::path MeasuresGoldenDir() {
  return std::filesystem::path(SPEAK5220_GOLDEN_DIR) / "quality" / "measures";
}

core::StatusOr<std::map<std::string, ReferenceMeasures>> ReadMeasureScores(
    const std::filesystem::path& path) {
  core::StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(path);
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  std::map<std::string, ReferenceMeasures> out;
  for (const auto& row : *rows) {
    ReferenceMeasures m;
    m.name = row.at("name");
    m.lag = std::stoi(row.at("lag"));
    m.speech_frames = std::stoi(row.at("speech_frames"));
    m.lsd_db = Number(row.at("lsd_db"));
    m.lsd_over_2db = Number(row.at("lsd_over_2db"));
    m.lsd_over_4db = Number(row.at("lsd_over_4db"));
    m.reaper_gpe = Number(row.at("reaper_gpe"));
    m.reaper_f0_rmse_cents = Number(row.at("reaper_f0_rmse_cents"));
    m.reaper_vde = Number(row.at("reaper_vde"));
    m.harvest_gpe = Number(row.at("harvest_gpe"));
    m.harvest_f0_rmse_cents = Number(row.at("harvest_f0_rmse_cents"));
    m.harvest_vde = Number(row.at("harvest_vde"));
    m.estoi = Number(row.at("estoi"));
    out[m.name] = std::move(m);
  }
  return out;
}

core::StatusOr<std::map<std::string, std::vector<bool>>> ReadMasks(
    const std::filesystem::path& path) {
  core::StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(path);
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  std::map<std::string, std::vector<bool>> out;
  for (const auto& row : *rows) {
    std::vector<bool>& mask = out[row.at("name")];
    for (const char c : row.at("mask")) {
      mask.push_back(c == '1');
    }
  }
  return out;
}

core::StatusOr<ReferenceTracks> ReadTracks(const std::filesystem::path& path) {
  core::StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(path);
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  ReferenceTracks tracks;
  // A track shorter than the others is padded with nan; its end is cut.
  const auto add = [](std::vector<double>& track, const std::string& text) {
    if (text != "nan") {
      track.push_back(std::stod(text));
    }
  };
  for (const auto& row : *rows) {
    add(tracks.reaper_original, row.at("reaper_original"));
    add(tracks.reaper_output, row.at("reaper_output"));
    add(tracks.harvest_original, row.at("harvest_original"));
    add(tracks.harvest_output, row.at("harvest_output"));
  }
  return tracks;
}

core::StatusOr<std::vector<PreparedEncoding>> PrepareReferenceEncodings() {
  const auto sets = ReadOptionSets(EncoderGoldenDir() / "option_sets.tsv");
  if (!sets.IsOk()) {
    return sets.GetStatus();
  }
  std::map<std::string, std::vector<double>> sources;
  for (const auto& entry :
       std::filesystem::directory_iterator(EncoderGoldenDir() / "inputs")) {
    if (entry.path().extension() != ".wav") {
      continue;
    }
    core::StatusOr<WavSamples> wav = ReadWavSamples(entry.path());
    if (!wav.IsOk()) {
      return wav.GetStatus();
    }
    sources[entry.path().stem().string()] = std::move(wav->samples);
  }
  core::synth::Synth synth;
  core::quality::FvadVoiceActivity vad(core::quality::kSpeechDetectorMode);
  std::vector<PreparedEncoding> prepared;
  for (const auto& [set_name, options] : *sets) {
    const auto encodings = ReadReferenceEncodings(
        EncoderGoldenDir() / "frames" / (set_name + ".tsv"));
    if (!encodings.IsOk()) {
      return encodings.GetStatus();
    }
    for (const auto& [name, source] : sources) {
      const auto encoding = encodings->find(name);
      if (encoding == encodings->end()) {
        return core::NotFoundError("no reference encoding of " + name);
      }
      core::StatusOr<core::quality::EncodedAudio> audio =
          core::quality::RenderEncodedAudio(synth, source,
                                            encoding->second.frames);
      if (!audio.IsOk()) {
        return audio.GetStatus();
      }
      core::StatusOr<core::quality::PreparedSignals> signals_or =
          core::quality::PrepareSignals(source, *audio, vad);
      if (!signals_or.IsOk()) {
        return signals_or.GetStatus();
      }
      // speechqual.py's lag counts from the first non-idle code.
      const auto first = std::ranges::find_if(
          audio->samples, [](std::int16_t s) { return s != -64; });
      const int first_sound = static_cast<int>(first - audio->samples.begin());
      prepared.push_back({.set_name = set_name,
                          .name = name,
                          .lag = first_sound - audio->source_offset,
                          .prepared = std::move(*signals_or)});
    }
  }
  return prepared;
}

}  // namespace speak5220::testing
