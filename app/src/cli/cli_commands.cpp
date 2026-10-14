/*
 * File:        cli_commands.cpp
 * Module:      cli
 * Purpose:     The speak5220-cli commands: new, add, import-phrom, encode,
 *              export, synth and measure
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "cli/cli_commands.h"

#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/dsp/resampler.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/lpc/frame_codec.h"
#include "core/preprocess/preprocess.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/phrase_importer.h"
#include "core/project/phrom_import.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/project/project_serializer.h"
#include "core/project/project_store.h"
#include "core/quality/loudness.h"
#include "core/quality/quality_assessor.h"
#include "core/synth/reference_synthesizer.h"
#include "core/synth/synth.h"
#include "core/util/file_digest.h"
#include "core/util/logging.h"
#include "core/util/status.h"

namespace speak5220::cli {
namespace {

namespace fs = std::filesystem;
namespace project = core::project;

const QCommandLineOption& NameOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("name"),
      QStringLiteral("new: the project's name. add: the phrase's name (one "
                     "file only)."),
      QStringLiteral("name"));
  return kOption;
}

const QCommandLineOption& EncoderOptionOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("option"),
      QStringLiteral("new: set a default encoder option, as KEY=VALUE "
                     "(repeatable)."),
      QStringLiteral("key=value"));
  return kOption;
}

const QCommandLineOption& DescriptionOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("description"),
      QStringLiteral("add: the phrase's description (one file only)."),
      QStringLiteral("text"));
  return kOption;
}

const QCommandLineOption& NormaliseOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("normalise"),
      QStringLiteral("add: normalise input that clips or is very quiet."));
  return kOption;
}

const QCommandLineOption& ForceOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("force"),
      QStringLiteral("encode: encode every phrase, not only stale ones."));
  return kOption;
}

const QCommandLineOption& OutputOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("output"),
      QStringLiteral("Write the project (new, add, import-phrom, encode), the "
                     "exported "
                     "files (export) or the measures (measure) into this "
                     "directory; synth: the WAV file to write."),
      QStringLiteral("dir"));
  return kOption;
}

const QCommandLineOption& PresetOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("preset"),
      QStringLiteral("export: run this export preset (repeatable)."),
      QStringLiteral("name"));
  return kOption;
}

const QCommandLineOption& FormatOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("format"),
      QStringLiteral("export: export in this format instead of the presets "
                     "(acorn-phrom, c-header-phrom, c-header-speak-external, "
                     "wav or bbc-dsd)."),
      QStringLiteral("format"));
  return kOption;
}

const QCommandLineOption& LayoutOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("layout"),
      QStringLiteral("export --format acorn-phrom: separate (default) or "
                     "concatenated."),
      QStringLiteral("layout"));
  return kOption;
}

const QCommandLineOption& PhraseOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("phrase"),
      QStringLiteral("export --format: export only this phrase "
                     "(repeatable)."),
      QStringLiteral("name"));
  return kOption;
}

const QCommandLineOption& LicenceOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("licence"),
      QStringLiteral("export --format c-header-...: the SPDX licence of the "
                     "header (default GPL-3.0-or-later)."),
      QStringLiteral("spdx"));
  return kOption;
}

const QCommandLineOption& AudioOption() {
  static const QCommandLineOption kOption(
      QStringLiteral("audio"),
      QStringLiteral("export --format wav: encoded (default), original or "
                     "preprocessed."),
      QStringLiteral("audio"));
  return kOption;
}

QString ToQString(std::string_view text) {
  return QString::fromUtf8(text.data(), static_cast<qsizetype>(text.size()));
}

fs::path ToPath(const QString& text) { return {text.toStdU16String()}; }

std::string PathText(const fs::path& path) {
  return QString::fromStdU16String(path.u16string()).toStdString();
}

QTextStream& Out() {
  static QTextStream stream(stdout);
  return stream;
}

QTextStream& Err() {
  static QTextStream stream(stderr);
  return stream;
}

int Fail(std::string_view message) {
  Err() << "speak5220-cli: " << ToQString(message) << Qt::endl;
  return kExitFailure;
}

int Fail(const core::Status& status) { return Fail(status.Message()); }

int Usage(std::string_view message) {
  Err() << "speak5220-cli: " << ToQString(message) << Qt::endl;
  return kExitUsage;
}

// A project path without a trailing separator, so a directory's name can
// be taken from it.
fs::path Normalised(const fs::path& path) {
  fs::path result = path.lexically_normal();
  if (!result.has_filename() && result.has_parent_path()) {
    result = result.parent_path();
  }
  return result;
}

// Where a command that changes the project writes it: in place, or into
// --output with the same name.
fs::path OutputProjectPath(const QCommandLineParser& parser,
                           const fs::path& project_path) {
  if (!parser.isSet(OutputOption())) {
    return project_path;
  }
  return ToPath(parser.value(OutputOption())) / project_path.filename();
}

core::StatusOr<project::Project> LoadProject(const fs::path& path) {
  core::audio::SndfileWavCodec codec;
  project::ProjectSerializer serializer(codec);
  const std::unique_ptr<project::IProjectStore> store =
      project::MakeProjectStore(path);
  core::StatusOr<project::Project> loaded = serializer.Load(*store);
  if (!loaded.IsOk()) {
    return core::Status(
        loaded.GetStatus().Code(),
        "cannot open " + PathText(path) + ": " + loaded.GetStatus().Message());
  }
  return loaded;
}

core::Status SaveProject(const project::Project& project,
                         const fs::path& path) {
  std::error_code error;
  if (path.has_parent_path()) {
    fs::create_directories(path.parent_path(), error);
    if (error) {
      return core::UnavailableError("cannot create " +
                                    PathText(path.parent_path()) + ": " +
                                    error.message());
    }
  }
  core::audio::SndfileWavCodec codec;
  project::ProjectSerializer serializer(codec);
  const std::unique_ptr<project::IProjectStore> store =
      project::MakeProjectStore(path);
  return serializer.Save(project, *store);
}

int RunNew(const QCommandLineParser& parser, const QStringList& arguments) {
  if (arguments.size() != 1) {
    return Usage(
        "usage: speak5220-cli new PROJECT [--name NAME] "
        "[--option KEY=VALUE]...");
  }
  const fs::path input = Normalised(ToPath(arguments.front()));
  const fs::path output = OutputProjectPath(parser, input);
  std::error_code error;
  if (fs::exists(output, error)) {
    return Fail(PathText(output) + " already exists");
  }
  project::Project created;
  project::ProjectSettings settings = created.Settings();
  settings.name = parser.isSet(NameOption())
                      ? parser.value(NameOption()).toStdString()
                      : PathText(output.stem());
  for (const QString& item : parser.values(EncoderOptionOption())) {
    const qsizetype equals = item.indexOf(QLatin1Char('='));
    if (equals <= 0) {
      return Usage("--option takes KEY=VALUE, not \"" + item.toStdString() +
                   "\"");
    }
    if (core::Status status = core::encoder::SetEncoderOption(
            settings.default_encoder, item.left(equals).toStdString(),
            item.mid(equals + 1).toStdString());
        !status.IsOk()) {
      return Fail("--option " + item.toStdString() + ": " + status.Message());
    }
  }
  if (core::Status status = created.SetSettings(std::move(settings));
      !status.IsOk()) {
    return Fail(status);
  }
  if (core::Status status = SaveProject(created, output); !status.IsOk()) {
    return Fail(status);
  }
  Out() << "created " << ToQString(PathText(output)) << Qt::endl;
  return kExitSuccess;
}

int RunAdd(const QCommandLineParser& parser, const QStringList& arguments) {
  if (arguments.size() < 2) {
    return Usage(
        "usage: speak5220-cli add PROJECT FILE.wav... [--name NAME] "
        "[--description TEXT] [--normalise]");
  }
  if (parser.isSet(NameOption()) && arguments.size() != 2) {
    return Usage("--name names one phrase: give one file with it");
  }
  if (parser.isSet(DescriptionOption()) && arguments.size() != 2) {
    return Usage("--description describes one phrase: give one file with it");
  }
  const fs::path input = Normalised(ToPath(arguments.front()));
  core::StatusOr<project::Project> loaded = LoadProject(input);
  if (!loaded.IsOk()) {
    return Fail(loaded.GetStatus());
  }
  project::Project& edited = *loaded;

  core::audio::SndfileWavReader reader;
  core::dsp::SincResampler resampler;
  core::FileSha256Digest digest;
  project::PhraseImporter importer(reader, resampler, digest);
  std::set<std::string> taken = edited.PhraseNames();
  std::vector<project::PlacedPhrase> added;
  for (qsizetype i = 1; i < arguments.size(); ++i) {
    project::ImportRequest request{
        .path = ToPath(arguments[i]),
        .name = parser.isSet(NameOption())
                    ? std::optional(parser.value(NameOption()).toStdString())
                    : std::nullopt,
        .normalise = parser.isSet(NormaliseOption())};
    core::StatusOr<project::ImportResult> result =
        importer.Import(request, project::NewPhraseId(), taken);
    if (!result.IsOk()) {
      return Fail(result.GetStatus());
    }
    if (parser.isSet(DescriptionOption())) {
      result->phrase.description =
          parser.value(DescriptionOption()).toStdString();
    }
    const project::Phrase& phrase = result->phrase;
    if ((result->clipped || result->quiet) && !result->normalised) {
      Err() << "speak5220-cli: warning: " << ToQString(PathText(request.path))
            << (result->clipped ? " clips" : " is very quiet")
            << "; --normalise would bring its peak to -1 dBFS" << Qt::endl;
    }
    Out() << "added " << ToQString(phrase.name) << " from "
          << ToQString(PathText(request.path.filename())) << Qt::endl;
    taken.insert(phrase.name);
    added.push_back(
        {.index = static_cast<int>(edited.Phrases().size() + added.size()),
         .phrase = std::move(result->phrase)});
  }
  if (core::Status status = edited.InsertPhrases(std::move(added));
      !status.IsOk()) {
    return Fail(status);
  }
  if (core::Status status =
          SaveProject(edited, OutputProjectPath(parser, input));
      !status.IsOk()) {
    return Fail(status);
  }
  return kExitSuccess;
}

int RunEncode(const QCommandLineParser& parser, const QStringList& arguments) {
  if (arguments.size() != 1) {
    return Usage("usage: speak5220-cli encode PROJECT [--force]");
  }
  const fs::path input = Normalised(ToPath(arguments.front()));
  core::StatusOr<project::Project> loaded = LoadProject(input);
  if (!loaded.IsOk()) {
    return Fail(loaded.GetStatus());
  }
  project::Project& edited = *loaded;
  const std::unique_ptr<core::encoder::IEncoder> encoder =
      core::encoder::MakeEncoder();
  project::EncodingCache cache(*encoder);
  const project::RefreshReport report =
      cache.Refresh(edited, parser.isSet(ForceOption()));
  for (const project::RefreshFailure& failure : report.failures) {
    const project::Phrase* phrase = edited.FindPhrase(failure.id);
    Err() << "speak5220-cli: " << ToQString(phrase->name) << ": "
          << ToQString(failure.status.Message()) << Qt::endl;
  }
  if (core::Status status =
          SaveProject(edited, OutputProjectPath(parser, input));
      !status.IsOk()) {
    return Fail(status);
  }
  const project::PhromAllocation allocation = edited.Allocation();
  Out() << "encoded " << report.encoded << " phrase(s); "
        << allocation.PhromsUsed() << " PHROM(s) of " << project::kPhromBudget
        << ", " << allocation.FreeInLastPhrom() << " bytes free in the last"
        << Qt::endl;
  return report.failures.empty() ? kExitSuccess : kExitFailure;
}

int RunExport(const QCommandLineParser& parser, const QStringList& arguments) {
  if (arguments.size() != 1 || !parser.isSet(OutputOption())) {
    return Usage(
        "usage: speak5220-cli export PROJECT --output DIR [--preset NAME]... "
        "| [--format FORMAT [--phrase NAME]... [--layout LAYOUT] "
        "[--licence SPDX] [--audio encoded|original|preprocessed]]");
  }
  if (parser.isSet(FormatOption()) && parser.isSet(PresetOption())) {
    return Usage("give --preset or --format, not both");
  }
  const bool format_options =
      parser.isSet(PhraseOption()) || parser.isSet(LayoutOption()) ||
      parser.isSet(LicenceOption()) || parser.isSet(AudioOption());
  if (format_options && !parser.isSet(FormatOption())) {
    return Usage("--phrase, --layout, --licence and --audio need --format");
  }
  const fs::path input = Normalised(ToPath(arguments.front()));
  core::StatusOr<project::Project> loaded = LoadProject(input);
  if (!loaded.IsOk()) {
    return Fail(loaded.GetStatus());
  }
  const project::Project& opened = *loaded;

  // The presets to run, each with the base name of its files.
  std::vector<std::pair<project::ExportPreset, std::string>> runs;
  if (parser.isSet(FormatOption())) {
    project::ExportPreset preset;
    preset.name = "command line";
    preset.format = parser.value(FormatOption()).toStdString();
    for (const QString& name : parser.values(PhraseOption())) {
      preset.phrases.push_back(name.toStdString());
    }
    preset.scope = std::string(
        preset.phrases.empty() ? project::kScopeAll : project::kScopeSelected);
    preset.layout = parser.value(LayoutOption()).toStdString();
    preset.licence = parser.value(LicenceOption()).toStdString();
    preset.audio = parser.value(AudioOption()).toStdString();
    runs.emplace_back(std::move(preset), PathText(input.stem()));
  } else if (parser.isSet(PresetOption())) {
    for (const QString& name : parser.values(PresetOption())) {
      const std::string wanted = name.toStdString();
      bool found = false;
      for (const project::ExportPreset& preset : opened.Contents().exports) {
        if (preset.name == wanted) {
          runs.emplace_back(preset, preset.name);
          found = true;
        }
      }
      if (!found) {
        return Fail("the project has no export preset called \"" + wanted +
                    "\"");
      }
    }
  } else {
    for (const project::ExportPreset& preset : opened.Contents().exports) {
      runs.emplace_back(preset, preset.name);
    }
    if (runs.empty()) {
      return Fail("the project has no export presets: give --format");
    }
  }
  for (const auto& [preset, base] : runs) {
    if (core::Status status = project::CheckExportPreset(preset);
        !status.IsOk()) {
      return Fail("export \"" + preset.name + "\": " + status.Message());
    }
    if (core::Status status = project::CheckEncodingsCurrent(opened, preset);
        !status.IsOk()) {
      return Fail("export \"" + preset.name + "\": " + status.Message() +
                  ": run \"speak5220-cli encode\" first");
    }
  }

  const fs::path directory = ToPath(parser.value(OutputOption()));
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::audio::SndfileWavCodec wav_codec;
  const project::ExportTools tools{.synth = &synth, .wav_codec = &wav_codec};
  for (const auto& [preset, base] : runs) {
    core::StatusOr<std::vector<project::ExportedFile>> files =
        project::RunExport(opened, preset, base, tools);
    if (!files.IsOk()) {
      return Fail("export \"" + preset.name +
                  "\": " + files.GetStatus().Message());
    }
    if (core::Status status = project::WriteExportedFiles(directory, *files);
        !status.IsOk()) {
      return Fail(status);
    }
    for (const project::ExportedFile& file : *files) {
      Out() << "wrote " << ToQString(PathText(directory / file.name))
            << Qt::endl;
    }
  }
  return kExitSuccess;
}

// Reads a whole file.
core::StatusOr<std::vector<std::uint8_t>> ReadFileBytes(const fs::path& path) {
  std::ifstream in(path, std::ios::binary);
  if (!in) {
    return core::NotFoundError("cannot open " + PathText(path));
  }
  std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                  std::istreambuf_iterator<char>());
  if (in.bad()) {
    return core::UnavailableError("cannot read " + PathText(path));
  }
  return bytes;
}

int RunImportPhrom(const QCommandLineParser& parser,
                   const QStringList& arguments) {
  if (arguments.size() < 2) {
    return Usage("usage: speak5220-cli import-phrom PROJECT IMAGE...");
  }
  const fs::path input = Normalised(ToPath(arguments.front()));
  core::StatusOr<project::Project> loaded = LoadProject(input);
  if (!loaded.IsOk()) {
    return Fail(loaded.GetStatus());
  }
  project::Project& edited = *loaded;

  std::set<std::string> taken = edited.PhraseNames();
  // Every image's words, ASCII entries and header strings, in order.
  project::PhromImport all;
  for (qsizetype i = 1; i < arguments.size(); ++i) {
    const fs::path path = ToPath(arguments[i]);
    core::StatusOr<std::vector<std::uint8_t>> bytes = ReadFileBytes(path);
    if (!bytes.IsOk()) {
      return Fail(bytes.GetStatus());
    }
    core::StatusOr<project::PhromImport> imported =
        project::ImportPhromImage(*bytes, taken);
    if (!imported.IsOk()) {
      return Fail(PathText(path) + ": " + imported.GetStatus().Message());
    }
    for (project::Phrase& phrase : imported->phrases) {
      Out() << "added " << ToQString(phrase.name) << " from "
            << ToQString(PathText(path.filename())) << Qt::endl;
      taken.insert(phrase.name);
      all.phrases.push_back(std::move(phrase));
    }
    all.ascii_map.merge(imported->ascii_map);
    for (std::vector<std::string>& strings : imported->header_strings) {
      all.header_strings.push_back(std::move(strings));
    }
  }
  // The images' ASCII entries and header strings first, so the budget is
  // checked with them (plan §3.4).
  if (core::StatusOr<project::PhromSettings> previous =
          edited.SetPhromSettings(project::MergeImportedPhromSettings(
              edited.Settings(), edited.Phrases().empty(), all));
      !previous.IsOk()) {
    return Fail(previous.GetStatus());
  }
  std::vector<project::PlacedPhrase> added;
  added.reserve(all.phrases.size());
  for (project::Phrase& phrase : all.phrases) {
    added.push_back(
        {.index = static_cast<int>(edited.Phrases().size() + added.size()),
         .phrase = std::move(phrase)});
  }
  const std::size_t count = added.size();
  if (core::Status status = edited.InsertPhrases(std::move(added));
      !status.IsOk()) {
    return Fail(status);
  }
  if (core::Status status =
          SaveProject(edited, OutputProjectPath(parser, input));
      !status.IsOk()) {
    return Fail(status);
  }
  Out() << "imported " << count << " word(s) as locked phrases" << Qt::endl;
  return kExitSuccess;
}

int RunSynth(const QCommandLineParser& parser, const QStringList& arguments) {
  if (arguments.size() != 1 || !parser.isSet(OutputOption())) {
    return Usage("usage: speak5220-cli synth FRAMES.yaml --output FILE.wav");
  }
  const fs::path input = ToPath(arguments.front());
  const fs::path output = ToPath(parser.value(OutputOption()));
  core::StatusOr<std::vector<std::uint8_t>> bytes = ReadFileBytes(input);
  if (!bytes.IsOk()) {
    return Fail(bytes.GetStatus());
  }
  core::StatusOr<project::CachedEncoding> read =
      project::ReadFramesYaml(*bytes, PathText(input));
  if (!read.IsOk()) {
    return Fail(read.GetStatus());
  }
  const std::vector<core::lpc::Frame> frames =
      core::lpc::EnsureStop(read->frames);
  core::StatusOr<std::vector<std::uint8_t>> packed =
      core::lpc::FrameCodec::Pack(frames);
  if (!packed.IsOk()) {
    return Fail(PathText(input) + ": " + packed.GetStatus().Message());
  }
  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::StatusOr<core::synth::SynthResult> spoken =
      synth.Synthesize(*packed, core::synth::SynthPath::kVsm);
  if (!spoken.IsOk()) {
    return Fail(spoken.GetStatus());
  }
  // The I/O codes x 64 (synth::kSampleScale), before the DAC clips them:
  // the reference emulator's --wav convention.
  constexpr float kInt16Scale = 32768.0F;
  std::vector<float> samples(spoken->samples.size());
  for (std::size_t i = 0; i < samples.size(); ++i) {
    samples[i] = static_cast<float>(spoken->samples[i]) / kInt16Scale;
  }
  core::audio::SndfileWavWriter writer;
  if (core::Status status =
          writer.Write(output, samples, project::kProjectSampleRate,
                       core::audio::WavOutputFormat::kPcm16);
      !status.IsOk()) {
    return Fail(status);
  }
  Out() << "wrote " << ToQString(PathText(output)) << " (" << frames.size()
        << " frames, " << samples.size() << " samples)" << Qt::endl;
  return kExitSuccess;
}

// The measures file's columns (docs/application/reference/cli-options.md).
constexpr std::string_view kMeasuresHeader =
    "name\tsource\tframes\tbytes\tscore\tenvelope_score\tpitch_score\t"
    "intelligibility_score\tlsd_db\tlsd_over_2db\tlsd_over_4db\t"
    "gross_pitch_error\tf0_rmse_cents\tvoicing_error\testoi\tspeech_frames\t"
    "sounding_rms\tpeak\tclipped_percent\twrapped_samples\tbbc_rms\t"
    "a_weighted_rms";

int RunMeasure(const QCommandLineParser& parser, const QStringList& arguments) {
  if (arguments.size() != 1 || !parser.isSet(OutputOption())) {
    return Usage("usage: speak5220-cli measure PROJECT --output DIR");
  }
  const fs::path input = Normalised(ToPath(arguments.front()));
  core::StatusOr<project::Project> loaded = LoadProject(input);
  if (!loaded.IsOk()) {
    return Fail(loaded.GetStatus());
  }
  const project::Project& opened = *loaded;
  for (const project::Phrase& phrase : opened.Phrases()) {
    if (!phrase.encoding.has_value()) {
      return Fail(phrase.name +
                  " has no encoding: run \"speak5220-cli encode\" first");
    }
  }
  const fs::path directory = ToPath(parser.value(OutputOption()));
  std::error_code error;
  fs::create_directories(directory, error);
  if (error) {
    return Fail("cannot create " + PathText(directory) + ": " +
                error.message());
  }
  const fs::path report_path = directory / "measures.tsv";
  std::ofstream report(report_path);
  if (!report) {
    return Fail("cannot write " + PathText(report_path));
  }
  report << kMeasuresHeader << '\n';
  // Enough digits to compare runs and reproduce the corpus checks.
  constexpr int kDigits = 10;
  report << std::setprecision(kDigits);

  core::synth::Synth synth({}, core::synth::SynthCache::kOff);
  core::quality::QualityAssessor assessor(synth);
  constexpr double kPercent = 100.0;
  int measured = 0;
  for (const project::Phrase& phrase : opened.Phrases()) {
    if (!phrase.encoding.has_value()) {
      continue;  // Refused above.
    }
    const std::vector<core::lpc::Frame>& frames = phrase.encoding->frames;
    core::StatusOr<int> bytes = project::PackedSize(frames);
    if (!bytes.IsOk()) {
      return Fail(phrase.name + ": " + bytes.GetStatus().Message());
    }
    report << phrase.name << '\t'
           << (phrase.source.has_value() ? phrase.source->file : "") << '\t'
           << frames.size() << '\t' << *bytes << '\t';
    // The quality against the preprocessed source, as the encoder panel
    // measures it; blank for a phrase made from frames alone or with too
    // little speech.
    std::optional<core::quality::QualityAssessment> quality;
    if (!phrase.audio.empty()) {
      core::StatusOr<core::encoder::EncoderOptions> options =
          project::EffectiveEncoderOptions(opened.Settings().default_encoder,
                                           phrase.encoder);
      if (!options.IsOk()) {
        return Fail(phrase.name + ": " + options.GetStatus().Message());
      }
      const std::vector<double> source =
          core::preprocess::Preprocess(
              std::vector<double>(phrase.audio.begin(), phrase.audio.end()),
              options->preprocess)
              .samples;
      core::StatusOr<core::quality::QualityAssessment> assessed =
          assessor.Assess(source, frames);
      if (assessed.IsOk()) {
        quality = *assessed;
      } else {
        Err() << "speak5220-cli: warning: " << ToQString(phrase.name)
              << ": cannot measure the quality: "
              << ToQString(assessed.GetStatus().Message()) << Qt::endl;
      }
    }
    if (quality.has_value()) {
      const core::quality::QualityMeasurement& m = quality->measures;
      const core::quality::QualityScore& s = quality->score;
      report << s.overall << '\t' << s.components.envelope << '\t'
             << s.components.pitch << '\t';
      if (s.components.intelligibility.has_value()) {
        report << *s.components.intelligibility;
      }
      report << '\t' << m.envelope.mean_db << '\t' << m.envelope.over_2db
             << '\t' << m.envelope.over_4db << '\t' << m.pitch.gross_error
             << '\t' << m.pitch.f0_rmse_cents << '\t' << m.pitch.voicing_error
             << '\t';
      if (m.estoi.has_value()) {
        report << *m.estoi;
      }
      report << '\t' << m.speech_frames << '\t';
    } else {
      report << "\t\t\t\t\t\t\t\t\t\t\t\t";
    }
    const core::synth::ReferenceSynthesis spoken =
        core::synth::ReferenceSynthesizer::Synthesize(frames);
    const core::quality::Loudness loudness = core::quality::MeasureLoudness(
        spoken.codes, frames, spoken.wrapped_samples);
    report << loudness.sounding_rms << '\t' << loudness.peak << '\t'
           << kPercent * loudness.clipped << '\t' << loudness.wrapped_samples
           << '\t' << loudness.high_passed_rms << '\t'
           << loudness.a_weighted_rms << '\n';
    ++measured;
  }
  if (!report) {
    return Fail("cannot write " + PathText(report_path));
  }
  Out() << "measured " << measured << " phrase(s) into "
        << ToQString(PathText(report_path)) << Qt::endl;
  return kExitSuccess;
}

}  // namespace

void AddCommandOptions(QCommandLineParser& parser) {
  parser.addPositionalArgument(
      QStringLiteral("command"),
      QStringLiteral("new, add, import-phrom, encode, export, synth or "
                     "measure. With none, prints the version."),
      QStringLiteral("[command]"));
  parser.addPositionalArgument(
      QStringLiteral("arguments"),
      QStringLiteral("The project (a .s5proj file, or a directory for the "
                     "unpacked form), then for add the WAV files, for "
                     "import-phrom the PHROM images; for synth, a frames.yaml "
                     "file."),
      QStringLiteral("[arguments...]"));
  parser.addOption(NameOption());
  parser.addOption(EncoderOptionOption());
  parser.addOption(DescriptionOption());
  parser.addOption(NormaliseOption());
  parser.addOption(ForceOption());
  parser.addOption(OutputOption());
  parser.addOption(PresetOption());
  parser.addOption(FormatOption());
  parser.addOption(LayoutOption());
  parser.addOption(PhraseOption());
  parser.addOption(LicenceOption());
  parser.addOption(AudioOption());
}

int RunCommand(const QCommandLineParser& parser, const QStringList& arguments) {
  const QString& command = arguments.front();
  const QStringList rest = arguments.mid(1);
  core::Logger(core::kLogModuleCli)
      ->debug("command {} with {} argument(s)", command.toStdString(),
              rest.size());
  if (command == QStringLiteral("new")) {
    return RunNew(parser, rest);
  }
  if (command == QStringLiteral("add")) {
    return RunAdd(parser, rest);
  }
  if (command == QStringLiteral("import-phrom")) {
    return RunImportPhrom(parser, rest);
  }
  if (command == QStringLiteral("encode")) {
    return RunEncode(parser, rest);
  }
  if (command == QStringLiteral("export")) {
    return RunExport(parser, rest);
  }
  if (command == QStringLiteral("synth")) {
    return RunSynth(parser, rest);
  }
  if (command == QStringLiteral("measure")) {
    return RunMeasure(parser, rest);
  }
  return Usage("unknown command \"" + command.toStdString() +
               "\": use new, add, import-phrom, encode, export, synth or "
               "measure");
}

}  // namespace speak5220::cli
