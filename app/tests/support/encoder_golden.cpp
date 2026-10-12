/*
 * File:        encoder_golden.cpp
 * Module:      test support
 * Purpose:     Reads the encoder's reference data in app/golden/encoder/
 *              and compares encodings with it
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/encoder_golden.h"

#include <QByteArray>
#include <QFile>
#include <QIODevice>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QString>
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <map>
#include <set>
#include <span>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/frame_endings.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/util/status.h"
#include "support/golden_data.h"

namespace speak5220::testing {
namespace {

using core::StatusOr;
using core::encoder::EncoderOptions;

core::Status SetOption(EncoderOptions& options, const QString& key,
                       const QJsonValue& value) {
  if (key == "targetRms") {
    options.target_rms = value.toDouble();
  } else if (key == "repeatLambda") {
    options.repeat_lambda = value.toDouble();
  } else if (key == "gateDb") {
    options.gate_db = value.toDouble();
  } else if (key == "preEmphasis") {
    options.pre_emphasis =
        value.isString()
            ? core::encoder::PreEmphasis::Chirp()
            : core::encoder::PreEmphasis::FirstOrder(value.toDouble());
  } else if (key == "window") {
    options.window = value.toInt();
  } else if (key == "lagWindowHz") {
    options.lag_window_hz = value.toDouble();
  } else if (key == "voicingThreshold") {
    options.voicing_threshold = value.toDouble();
  } else if (key == "voicingEvidence") {
    options.voicing_evidence = value.toDouble();
  } else if (key == "levelAgc") {
    options.level_agc = value.toDouble();
  } else {
    return core::InvalidArgumentError("unknown encoder option " +
                                      key.toStdString());
  }
  return core::OkStatus();
}

StatusOr<std::vector<std::uint8_t>> FromHex(const std::string& hex) {
  if (hex.size() % 2 != 0) {
    return core::DataLossError("odd-length hex string");
  }
  std::vector<std::uint8_t> bytes;
  for (std::size_t i = 0; i < hex.size(); i += 2) {
    bytes.push_back(
        static_cast<std::uint8_t>(std::stoi(hex.substr(i, 2), nullptr, 16)));
  }
  return bytes;
}

}  // namespace

EncoderOptions ReferenceEncoderOptions() {
  EncoderOptions options;
  options.target_rms = 20.0;
  options.window = 240;
  options.voicing_evidence = 4.0;
  options.level_agc = 1.0;
  options.voicing_threshold = 0.3;
  options.resonance_hold_db.reset();
  options.voicing_min_run = 1;
  return options;
}

std::vector<core::lpc::Frame> ReferenceFrames(
    std::span<const core::lpc::Frame> reference) {
  return core::encoder::QuietPauses(reference);
}

std::filesystem::path EncoderGoldenDir() {
  return std::filesystem::path(SPEAK5220_GOLDEN_DIR) / "encoder";
}

StatusOr<std::map<std::string, EncoderOptions>> ReadOptionSets(
    const std::filesystem::path& path) {
  StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(path);
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  std::map<std::string, EncoderOptions> sets;
  for (const auto& row : *rows) {
    QJsonParseError error;
    const QJsonDocument doc = QJsonDocument::fromJson(
        QByteArray::fromStdString(row.at("options")), &error);
    if (doc.isNull() || !doc.isObject()) {
      return core::DataLossError(path.string() + ": bad options for " +
                                 row.at("name"));
    }
    EncoderOptions options = ReferenceEncoderOptions();
    const QJsonObject object = doc.object();
    for (auto it = object.begin(); it != object.end(); ++it) {
      if (core::Status status = SetOption(options, it.key(), it.value());
          !status.IsOk()) {
        return status;
      }
    }
    sets[row.at("name")] = options;
  }
  return sets;
}

StatusOr<std::map<std::string, ReferenceEncoding>> ReadReferenceEncodings(
    const std::filesystem::path& path) {
  StatusOr<std::vector<std::map<std::string, std::string>>> rows =
      ReadTable(path);
  if (!rows.IsOk()) {
    return rows.GetStatus();
  }
  std::map<std::string, ReferenceEncoding> out;
  for (const auto& row : *rows) {
    ReferenceEncoding ref;
    ref.name = row.at("name");
    StatusOr<std::vector<std::uint8_t>> bytes = FromHex(row.at("packed"));
    if (!bytes.IsOk()) {
      return bytes.GetStatus();
    }
    ref.frames = core::lpc::FrameCodec::Parse(*bytes).frames;
    if (std::to_string(ref.frames.size()) != row.at("frames")) {
      return core::DataLossError(path.string() + ": " + ref.name +
                                 " does not parse to its frame count");
    }
    if (row.at("near_ties") != "-") {
      std::stringstream ties(row.at("near_ties"));
      std::string item;
      while (std::getline(ties, item, ',')) {
        ref.near_ties.insert(static_cast<std::size_t>(std::stoul(item)));
      }
    }
    out[ref.name] = std::move(ref);
  }
  return out;
}

StatusOr<QJsonObject> ReadJsonObject(const std::filesystem::path& path) {
  QFile file(QString::fromStdString(path.string()));
  if (!file.open(QIODevice::ReadOnly)) {
    return core::NotFoundError("cannot read " + path.string());
  }
  QJsonParseError error;
  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
  if (!doc.isObject()) {
    return core::DataLossError(path.string() + ": " +
                               error.errorString().toStdString());
  }
  return doc.object();
}

std::vector<double> JsonDoubles(const QJsonArray& array) {
  std::vector<double> out;
  out.reserve(static_cast<std::size_t>(array.size()));
  for (const auto& v : array) {
    out.push_back(v.toDouble());
  }
  return out;
}

std::vector<int> JsonInts(const QJsonArray& array) {
  std::vector<int> out;
  out.reserve(static_cast<std::size_t>(array.size()));
  for (const auto& v : array) {
    out.push_back(v.toInt());
  }
  return out;
}

StatusOr<WavSamples> ReadWavSamples(const std::filesystem::path& path) {
  core::audio::SndfileWavReader reader;
  StatusOr<core::audio::DecodedWav> wav = reader.Read(path);
  if (!wav.IsOk()) {
    return wav.GetStatus();
  }
  return WavSamples{
      .sample_rate = wav->sample_rate,
      .samples = std::vector<double>(wav->samples.begin(), wav->samples.end())};
}

EncodingComparison CompareEncoding(
    const ReferenceEncoding& reference,
    const std::vector<core::lpc::Frame>& frames) {
  EncodingComparison out;
  const std::size_t n = std::max(reference.frames.size(), frames.size());
  for (std::size_t i = 0; i < n; ++i) {
    if (i >= reference.frames.size() || i >= frames.size() ||
        reference.frames[i] != frames[i]) {
      out.differing.push_back(i);
    }
  }
  out.identical = out.differing.empty();
  out.confined_to_near_ties = std::ranges::all_of(
      out.differing,
      [&](std::size_t i) { return reference.near_ties.contains(i); });
  return out;
}

}  // namespace speak5220::testing
