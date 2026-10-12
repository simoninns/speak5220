/*
 * File:        cli_commands.cpp
 * Module:      cli
 * Purpose:     The speak5220-cli commands: new, add, encode and export
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
#include <filesystem>
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
#include "core/project/encoding_cache.h"
#include "core/project/phrase.h"
#include "core/project/phrase_importer.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/project/project_serializer.h"
#include "core/project/project_store.h"
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
      QStringLiteral("Write the project (new, add, encode) or the exported "
                     "files (export) into this directory."),
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
      QStringLiteral("export --format wav: encoded (default) or original."),
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
    return Usage("usage: speak5220-cli new PROJECT [--name NAME]");
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
        "[--licence SPDX] [--audio encoded|original]]");
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

}  // namespace

void AddCommandOptions(QCommandLineParser& parser) {
  parser.addPositionalArgument(
      QStringLiteral("command"),
      QStringLiteral("new, add, encode or export. With none, prints the "
                     "version."),
      QStringLiteral("[command]"));
  parser.addPositionalArgument(
      QStringLiteral("arguments"),
      QStringLiteral("The project (a .s5proj file, or a directory for the "
                     "unpacked form), then for add the WAV files."),
      QStringLiteral("[arguments...]"));
  parser.addOption(NameOption());
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
  if (command == QStringLiteral("encode")) {
    return RunEncode(parser, rest);
  }
  if (command == QStringLiteral("export")) {
    return RunExport(parser, rest);
  }
  return Usage("unknown command \"" + command.toStdString() +
               "\": use new, add, encode or export");
}

}  // namespace speak5220::cli
