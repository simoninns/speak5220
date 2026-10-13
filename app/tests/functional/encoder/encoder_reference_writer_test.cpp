/*
 * File:        encoder_reference_writer_test.cpp
 * Module:      encoder tests
 * Purpose:     Writes the encoder's reference frames and level-setting
 *              intermediates for app/scripts/golden/encoder.py
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QByteArray>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QString>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/dsp/fft.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/encoder/encoder_types.h"
#include "core/encoder/level_setter.h"
#include "core/lpc/coding_tables.h"
#include "core/lpc/frame_codec.h"
#include "gtest/gtest.h"
#include "support/encoder_golden.h"

namespace speak5220::testing {
namespace {

using core::encoder::EncodeDetails;
using core::encoder::PlanEntry;
using core::encoder::PlanKind;

std::string Hex(std::span<const std::uint8_t> bytes) {
  constexpr std::string_view kDigits = "0123456789abcdef";
  std::string out;
  for (const std::uint8_t b : bytes) {
    out += kDigits[b >> 4];
    out += kDigits[b & 0xF];
  }
  return out;
}

// The WAV files of `dir`, by stem.
std::map<std::string, std::filesystem::path> WavFiles(
    const std::filesystem::path& dir) {
  std::map<std::string, std::filesystem::path> files;
  for (const auto& entry : std::filesystem::directory_iterator(dir)) {
    if (entry.path().extension() == ".wav") {
      files[entry.path().stem().string()] = entry.path();
    }
  }
  return files;
}

QString KindName(PlanKind kind) {
  switch (kind) {
    case PlanKind::kVoiced:
      return "voiced";
    case PlanKind::kUnvoiced:
      return "unvoiced";
    case PlanKind::kRepeat:
      return "repeat";
    case PlanKind::kSilence:
      break;
  }
  return "silence";
}

QJsonArray PlanJson(std::span<const PlanEntry> plan) {
  QJsonArray out;
  for (const PlanEntry& p : plan) {
    QJsonArray k;
    for (const int v : p.k) {
      k.append(v);
    }
    out.append(
        QJsonObject{{"kind", KindName(p.kind)}, {"k", k}, {"pitch", p.pitch}});
  }
  return out;
}

template <typename T>
QJsonArray ArrayJson(std::span<const T> values) {
  QJsonArray out;
  for (const T v : values) {
    out.append(v);
  }
  return out;
}

// The "encode" section of an intermediates file (app/golden/README.md):
// what the level setting made of the encoder's plan.
QJsonObject EncodeSection(core::dsp::IFft& fft, const EncodeDetails& details,
                          std::span<const std::uint8_t> packed,
                          std::size_t frame_count) {
  const std::vector<std::vector<double>> eff =
      core::encoder::EffectiveK(details.plan);
  std::vector<double> gains;
  for (std::size_t j = 0; j < details.plan.size(); ++j) {
    const PlanEntry& p = details.plan[j];
    if (p.kind == PlanKind::kSilence) {
      gains.push_back(0.0);
      continue;
    }
    const int period =
        p.pitch != 0
            ? core::lpc::kPitchTable.at(static_cast<std::size_t>(p.pitch)) + 1
            : 0;
    gains.push_back(
        core::encoder::GainOf(fft, eff[j], period, /*loudness=*/true));
  }
  return QJsonObject{
      {"lead", details.lead},
      {"dropped", details.dropped_opening},
      {"plan", PlanJson(details.plan)},
      {"levels", ArrayJson<double>(details.levels)},
      {"gain", details.gain},
      {"gains", ArrayJson<double>(gains)},
      {"energies", ArrayJson<int>(details.energies)},
      {"silenced", details.stats.silenced},
      {"limited", details.stats.limited},
      {"sounding", details.stats.sounding},
      {"frames_hex", QString::fromStdString(Hex(packed))},
      {"frame_count", static_cast<int>(frame_count)},
  };
}

// Run by app/scripts/golden/encoder.py, not by CTest. In
// SPEAK5220_ENCODER_REFERENCE_DIR, which holds option_sets.tsv and
// inputs/ already, it writes frames/SET.tsv for every WAV file of
// SPEAK5220_CORPUS_DIR (app/scripts/generate_corpus.py) and every option
// set, and encode/NAME.json, the "encode" section of each input's
// intermediates with the reference's defaults.
TEST(EncoderReferenceWriter, DISABLED_WritesFramesAndLevelSetting) {
  const char* corpus = std::getenv("SPEAK5220_CORPUS_DIR");
  const char* out_dir = std::getenv("SPEAK5220_ENCODER_REFERENCE_DIR");
  ASSERT_NE(corpus, nullptr) << "set SPEAK5220_CORPUS_DIR (encoder.py)";
  ASSERT_NE(out_dir, nullptr)
      << "set SPEAK5220_ENCODER_REFERENCE_DIR (encoder.py)";
  const std::filesystem::path out(out_dir);
  const auto sets = ReadOptionSets(out / "option_sets.tsv");
  ASSERT_TRUE(sets.IsOk()) << sets.GetStatus().ToString();
  const std::map<std::string, std::filesystem::path> files = WavFiles(corpus);
  ASSERT_FALSE(files.empty()) << "no WAV files in " << corpus;
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();

  std::filesystem::create_directories(out / "frames");
  for (const auto& [set_name, options] : *sets) {
    std::ofstream tsv(out / "frames" / (set_name + ".tsv"));
    ASSERT_TRUE(tsv.is_open()) << "cannot write frames/" << set_name;
    tsv << "name\tframes\tnear_ties\tpacked\n";
    for (const auto& [stem, path] : files) {
      const auto wav = ReadWavSamples(path);
      ASSERT_TRUE(wav.IsOk()) << stem << ": " << wav.GetStatus().ToString();
      const auto result =
          encoder->Encode(wav->samples, wav->sample_rate, options);
      ASSERT_TRUE(result.IsOk())
          << stem << ": " << result.GetStatus().ToString();
      const auto packed = core::lpc::FrameCodec::Pack(result->frames);
      ASSERT_TRUE(packed.IsOk()) << stem;
      tsv << stem << '\t' << result->frames.size() << "\t-\t" << Hex(*packed)
          << '\n';
    }
  }

  std::filesystem::create_directories(out / "encode");
  core::dsp::PocketFft fft;
  const std::map<std::string, std::filesystem::path> inputs =
      WavFiles(out / "inputs");
  ASSERT_FALSE(inputs.empty()) << "no inputs in " << out;
  for (const auto& [stem, path] : inputs) {
    const auto wav = ReadWavSamples(path);
    ASSERT_TRUE(wav.IsOk()) << stem << ": " << wav.GetStatus().ToString();
    const auto result = encoder->Encode(wav->samples, wav->sample_rate,
                                        ReferenceEncoderOptions());
    ASSERT_TRUE(result.IsOk()) << stem << ": " << result.GetStatus().ToString();
    ASSERT_TRUE(result->details.has_value()) << stem;
    const auto packed = core::lpc::FrameCodec::Pack(result->frames);
    ASSERT_TRUE(packed.IsOk()) << stem;
    const QJsonObject section =
        EncodeSection(fft, *result->details, *packed, result->frames.size());
    QFile file(
        QString::fromStdString((out / "encode" / (stem + ".json")).string()));
    ASSERT_TRUE(file.open(QIODevice::WriteOnly)) << "cannot write " << stem;
    file.write(QJsonDocument(section).toJson(QJsonDocument::Compact));
  }
}

}  // namespace
}  // namespace speak5220::testing
