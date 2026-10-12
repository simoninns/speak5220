/*
 * File:        project_export.cpp
 * Module:      project
 * Purpose:     Runs a project's export presets: Acorn PHROM images, C
 *              headers, WAV files and the BBC Micro disc
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_export.h"

#include <spdlog/fmt/fmt.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

#include "core/lpc/frame_codec.h"
#include "core/packaging/acorn_phrom.h"
#include "core/packaging/c_header.h"
#include "core/packaging/speak_external_stream.h"
#include "core/project/c_identifier.h"
#include "core/project/encoding_cache.h"
#include "core/project/phrom_allocator.h"
#include "core/synth/output_pin.h"
#include "core/util/logging.h"

namespace speak5220::core::project {
namespace {

constexpr std::array<ExportFormatInfo, 5> kFormats = {{
    {.key = kAcornPhromFormat,
     .label = "Acorn PHROM images",
     .takes_layout = true,
     .takes_licence = false,
     .takes_audio = false},
    {.key = kCHeaderPhromFormat,
     .label = "C header, PHROM images",
     .takes_layout = false,
     .takes_licence = true,
     .takes_audio = false},
    {.key = kCHeaderSpeakExternalFormat,
     .label = "C header, Speak External streams",
     .takes_layout = false,
     .takes_licence = true,
     .takes_audio = false},
    {.key = kWavFormat,
     .label = "WAV audio",
     .takes_layout = false,
     .takes_licence = false,
     .takes_audio = true},
    {.key = kBbcDiscFormat,
     .label = "BBC Micro disc",
     .takes_layout = false,
     .takes_licence = false,
     .takes_audio = false},
}};

// A 16-bit sample as a fraction of full scale, exact in a float.
constexpr float kInt16Scale = 32768.0F;

// Fails unless an option the format does not take is empty.
Status CheckUnused(const std::string& value, std::string_view option,
                   std::string_view format) {
  if (value.empty()) {
    return OkStatus();
  }
  return InvalidArgumentError(
      fmt::format(R"(the {} "{}" does not apply to the format "{}")", option,
                  value, format));
}

// The frames of a phrase the export needs encoded.
StatusOr<const std::vector<lpc::Frame>*> FramesOf(const Phrase& phrase) {
  if (!phrase.encoding.has_value() || phrase.encoding->frames.empty()) {
    return FailedPreconditionError(fmt::format(
        "{} has not been encoded yet, so it cannot be exported", phrase.name));
  }
  return &phrase.encoding->frames;
}

std::vector<std::string> NamesOf(std::span<const Phrase* const> phrases) {
  std::vector<std::string> names;
  names.reserve(phrases.size());
  for (const Phrase* phrase : phrases) {
    names.push_back(phrase->name);
  }
  return names;
}

std::string LowerCase(std::string text) {
  std::ranges::transform(text, text.begin(), [](char c) {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
  });
  return text;
}

packaging::CHeaderInfo HeaderInfo(const Project& project,
                                  const ExportPreset& preset,
                                  std::string_view base) {
  return {.prefix = LowerCase(CIdentifierFromName(base)),
          .project_name = project.Settings().name,
          .licence = preset.licence.empty()
                         ? std::string(kDefaultCHeaderLicence)
                         : preset.licence};
}

ExportedFile TextFile(std::string name, const std::string& text) {
  return {.name = std::move(name),
          .bytes = std::vector<std::uint8_t>(text.begin(), text.end())};
}

StatusOr<std::vector<ExportedFile>> AcornPhromFiles(
    const Project& project, const ExportPreset& preset,
    std::span<const Phrase* const> phrases, std::string_view base) {
  StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(project.Settings(), phrases);
  if (!images.IsOk()) {
    return images.GetStatus();
  }
  std::vector<ExportedFile> files;
  if (preset.layout == kLayoutConcatenated) {
    ExportedFile file{.name = fmt::format("{}.bin", base), .bytes = {}};
    for (const packaging::PhromImage& image : *images) {
      file.bytes.insert(file.bytes.end(), image.bytes.begin(),
                        image.bytes.end());
    }
    files.push_back(std::move(file));
  } else {
    for (std::size_t i = 0; i < images->size(); ++i) {
      files.push_back({.name = fmt::format("{}_{}.bin", base, i + 1),
                       .bytes = std::move((*images)[i].bytes)});
    }
  }
  return files;
}

StatusOr<std::vector<ExportedFile>> PhromHeaderFiles(
    const Project& project, const ExportPreset& preset,
    std::span<const Phrase* const> phrases, std::string_view base) {
  StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(project.Settings(), phrases);
  if (!images.IsOk()) {
    return images.GetStatus();
  }
  // The identifiers follow the phrases' order, which is the images' word
  // order.
  const std::vector<std::string> names = NamesOf(phrases);
  const std::vector<std::string> identifiers = CIdentifiersForNames(names);
  std::vector<packaging::CHeaderName> mapping;
  mapping.reserve(names.size());
  for (std::size_t i = 0; i < names.size(); ++i) {
    mapping.push_back({.name = names[i], .identifier = identifiers[i]});
  }
  StatusOr<std::string> text = packaging::CHeader::ForPhroms(
      HeaderInfo(project, preset, base), *images, mapping);
  if (!text.IsOk()) {
    return text.GetStatus();
  }
  return std::vector<ExportedFile>{TextFile(fmt::format("{}.h", base), *text)};
}

StatusOr<std::vector<ExportedFile>> StreamHeaderFiles(
    const Project& project, const ExportPreset& preset,
    std::span<const Phrase* const> phrases, std::string_view base) {
  const std::vector<std::string> names = NamesOf(phrases);
  const std::vector<std::string> identifiers = CIdentifiersForNames(names);
  std::vector<packaging::CHeaderStream> streams;
  streams.reserve(phrases.size());
  for (std::size_t i = 0; i < phrases.size(); ++i) {
    StatusOr<const std::vector<lpc::Frame>*> frames = FramesOf(*phrases[i]);
    if (!frames.IsOk()) {
      return frames.GetStatus();
    }
    StatusOr<std::vector<std::uint8_t>> bytes =
        packaging::SpeakExternalStream::Build(**frames);
    if (!bytes.IsOk()) {
      return Status(
          bytes.GetStatus().Code(),
          fmt::format("{}: {}", names[i], bytes.GetStatus().Message()));
    }
    streams.push_back({.name = {.name = names[i], .identifier = identifiers[i]},
                       .bytes = *std::move(bytes)});
  }
  StatusOr<std::string> text = packaging::CHeader::ForStreams(
      HeaderInfo(project, preset, base), streams);
  if (!text.IsOk()) {
    return text.GetStatus();
  }
  return std::vector<ExportedFile>{TextFile(fmt::format("{}.h", base), *text)};
}

// The chip's SPEAKER output for a phrase, as the encoded pane shows it
// with the Audio menu at its defaults.
StatusOr<std::vector<float>> EncodedSamples(synth::ISynth& synth,
                                            const Phrase& phrase) {
  StatusOr<const std::vector<lpc::Frame>*> frames = FramesOf(phrase);
  if (!frames.IsOk()) {
    return frames.GetStatus();
  }
  StatusOr<std::vector<std::uint8_t>> packed = lpc::FrameCodec::Pack(**frames);
  if (!packed.IsOk()) {
    return packed.GetStatus();
  }
  StatusOr<synth::SynthResult> spoken =
      synth.Synthesize(*packed, synth::SynthPath::kVsm);
  if (!spoken.IsOk()) {
    return spoken.GetStatus();
  }
  const std::vector<std::int16_t> pin =
      synth::PinSamples(synth::OutputPin::kSpeaker, spoken->samples);
  std::vector<float> samples(pin.size());
  std::ranges::transform(pin, samples.begin(), [](std::int16_t sample) {
    return static_cast<float>(sample) / kInt16Scale;
  });
  return samples;
}

StatusOr<std::vector<ExportedFile>> WavFiles(
    const ExportPreset& preset, std::span<const Phrase* const> phrases,
    std::string_view base, const ExportTools& tools) {
  const bool original = preset.audio == kAudioOriginal;
  if (tools.wav_codec == nullptr || (!original && tools.synth == nullptr)) {
    return FailedPreconditionError(
        "WAV export needs the chip and a WAV writer");
  }
  const std::vector<std::string> identifiers =
      CIdentifiersForNames(NamesOf(phrases));
  std::vector<ExportedFile> files;
  files.reserve(phrases.size());
  for (std::size_t i = 0; i < phrases.size(); ++i) {
    const Phrase& phrase = *phrases[i];
    StatusOr<std::vector<std::uint8_t>> bytes = std::vector<std::uint8_t>{};
    if (original) {
      if (phrase.audio.empty()) {
        return FailedPreconditionError(fmt::format(
            "{} has no source audio, so its original cannot be exported",
            phrase.name));
      }
      bytes = tools.wav_codec->Encode(phrase.audio, kProjectSampleRate,
                                      audio::WavOutputFormat::kPcm24);
    } else {
      StatusOr<std::vector<float>> samples =
          EncodedSamples(*tools.synth, phrase);
      if (!samples.IsOk()) {
        return samples.GetStatus();
      }
      bytes = tools.wav_codec->Encode(*samples, kProjectSampleRate,
                                      audio::WavOutputFormat::kPcm16);
    }
    if (!bytes.IsOk()) {
      return Status(
          bytes.GetStatus().Code(),
          fmt::format("{}: {}", phrase.name, bytes.GetStatus().Message()));
    }
    files.push_back({.name = fmt::format("{}_{}.wav", base, identifiers[i]),
                     .bytes = *std::move(bytes)});
  }
  return files;
}

StatusOr<std::vector<ExportedFile>> BbcDiscFiles(
    const Project& project, std::span<const Phrase* const> phrases,
    std::string_view base) {
  StatusOr<std::vector<bbc::BbcDiscPhrom>> phroms =
      BuildBbcDiscPhroms(project.Settings(), phrases);
  if (!phroms.IsOk()) {
    return phroms.GetStatus();
  }
  StatusOr<std::vector<std::uint8_t>> disc =
      bbc::BbcDisc::Build(project.Settings().name, *phroms);
  if (!disc.IsOk()) {
    return disc.GetStatus();
  }
  return std::vector<ExportedFile>{
      {.name = fmt::format("{}.dsd", base), .bytes = *std::move(disc)}};
}

}  // namespace

std::span<const ExportFormatInfo> ExportFormats() { return kFormats; }

const ExportFormatInfo* FindExportFormat(std::string_view key) {
  const auto* found = std::ranges::find(kFormats, key, &ExportFormatInfo::key);
  return found == kFormats.end() ? nullptr : found;
}

Status CheckExportPreset(const ExportPreset& preset) {
  const ExportFormatInfo* format = FindExportFormat(preset.format);
  if (format == nullptr) {
    std::string known;
    for (const ExportFormatInfo& info : kFormats) {
      known += fmt::format(R"({}"{}")", known.empty() ? "" : ", ", info.key);
    }
    return InvalidArgumentError(fmt::format(
        R"(export format "{}" is not one of {})", preset.format, known));
  }
  if (preset.scope == kScopeSelected) {
    if (preset.phrases.empty()) {
      return InvalidArgumentError(
          R"(export scope "selected" needs the phrases it exports)");
    }
  } else if (preset.scope.empty() || preset.scope == kScopeAll) {
    if (!preset.phrases.empty()) {
      return InvalidArgumentError(
          R"(phrases are listed only with the export scope "selected")");
    }
  } else {
    return InvalidArgumentError(
        fmt::format(R"(export scope "{}" is not "{}" or "{}")", preset.scope,
                    kScopeAll, kScopeSelected));
  }
  if (!format->takes_layout) {
    if (Status status = CheckUnused(preset.layout, "layout", format->key);
        !status.IsOk()) {
      return status;
    }
  } else if (!preset.layout.empty() && preset.layout != kLayoutSeparate &&
             preset.layout != kLayoutConcatenated) {
    return InvalidArgumentError(
        fmt::format(R"(PHROM layout "{}" is not "{}" or "{}")", preset.layout,
                    kLayoutSeparate, kLayoutConcatenated));
  }
  if (!format->takes_licence) {
    if (Status status = CheckUnused(preset.licence, "licence", format->key);
        !status.IsOk()) {
      return status;
    }
  } else if (!preset.licence.empty()) {
    // The prefix is checked when the header is written.
    if (Status status = packaging::CHeader::CheckInfo(
            {.prefix = "c", .project_name = {}, .licence = preset.licence});
        !status.IsOk()) {
      return status;
    }
  }
  if (!format->takes_audio) {
    if (Status status = CheckUnused(preset.audio, "audio", format->key);
        !status.IsOk()) {
      return status;
    }
  } else if (!preset.audio.empty() && preset.audio != kAudioEncoded &&
             preset.audio != kAudioOriginal) {
    return InvalidArgumentError(
        fmt::format(R"(WAV audio "{}" is not "{}" or "{}")", preset.audio,
                    kAudioEncoded, kAudioOriginal));
  }
  return OkStatus();
}

Status CheckExportBase(std::string_view base) {
  constexpr std::string_view kRefused = "/\\:*?\"<>|";
  const bool control = std::ranges::any_of(
      base, [](char c) { return static_cast<unsigned char>(c) < 0x20; });
  if (base.empty() || base == "." || base == ".." ||
      base.find_first_of(kRefused) != std::string_view::npos || control) {
    return InvalidArgumentError(fmt::format(
        R"(exported files cannot be named from "{}": use a name without )"
        R"(/ \ : * ? " < > |)",
        base));
  }
  return OkStatus();
}

StatusOr<std::vector<const Phrase*>> PhrasesInScope(
    const Project& project, const ExportPreset& preset) {
  std::vector<const Phrase*> phrases;
  if (preset.scope != kScopeSelected) {
    for (const Phrase& phrase : project.Phrases()) {
      phrases.push_back(&phrase);
    }
    return phrases;
  }
  const std::set<std::string> wanted(preset.phrases.begin(),
                                     preset.phrases.end());
  for (const std::string& name : wanted) {
    if (!std::ranges::any_of(project.Phrases(), [&](const Phrase& phrase) {
          return phrase.name == name;
        })) {
      return NotFoundError(fmt::format(
          "the export names a phrase the project does not have: {}", name));
    }
  }
  for (const Phrase& phrase : project.Phrases()) {
    if (wanted.contains(phrase.name)) {
      phrases.push_back(&phrase);
    }
  }
  return phrases;
}

std::vector<std::string> PhromHeaderStrings(const ProjectSettings& settings,
                                            int number) {
  for (const PhromHeader& phrom : settings.phroms) {
    if (phrom.number == number) {
      return phrom.header_strings;
    }
  }
  std::vector<std::string> strings = settings.header_strings;
  if (strings.size() > 1) {
    strings[1] += " " + std::to_string(number);
  }
  return strings;
}

StatusOr<std::vector<packaging::PhromImage>> BuildAcornPhroms(
    const ProjectSettings& settings, std::span<const Phrase* const> phrases) {
  std::vector<PhraseCostInput> costs;
  costs.reserve(phrases.size());
  for (const Phrase* phrase : phrases) {
    // Frames are validated whenever they enter the project.
    StatusOr<PhraseCostInput> cost = CostOfPhrase(*phrase);
    if (!cost.IsOk()) {
      return cost.GetStatus();
    }
    costs.push_back(*cost);
  }
  const PhromAllocation allocation = AllocatePhroms(costs);
  for (std::size_t i = 0; i < phrases.size(); ++i) {
    switch (allocation.phrases[i].placement) {
      case Placement::kPlaced:
        break;
      case Placement::kNotEncoded:
        return FailedPreconditionError(
            fmt::format("{} has not been encoded yet, so it cannot be exported",
                        phrases[i]->name));
      case Placement::kTooLarge:
        return ResourceExhaustedError(fmt::format(
            "{} needs {} bytes, more than an empty PHROM holds ({}): shorten "
            "or re-encode it",
            phrases[i]->name, allocation.phrases[i].cost, kPhromWordCapacity));
    }
  }
  std::vector<packaging::PhromImage> images;
  for (int number = 1; number <= allocation.PhromsUsed(); ++number) {
    const PhromFill& fill =
        allocation.phroms[static_cast<std::size_t>(number - 1)];
    std::vector<packaging::Word> words;
    std::set<std::string> names;
    for (const int index : fill.phrases) {
      const Phrase& phrase = *phrases[static_cast<std::size_t>(index)];
      // A placed phrase always has frames.
      if (!phrase.encoding.has_value()) {
        return InternalError(phrase.name + " was placed without frames");
      }
      words.push_back({.name = phrase.name, .frames = phrase.encoding->frames});
      names.insert(phrase.name);
    }
    packaging::AcornPhromOptions options;
    options.strings = PhromHeaderStrings(settings, number);
    // Plan §4.1: only the entries whose word is in this PHROM.
    for (const auto& [character, name] : settings.ascii_map) {
      if (names.contains(name)) {
        options.ascii_map[static_cast<int>(character)] = name;
      }
    }
    StatusOr<packaging::PhromImage> image =
        packaging::AcornPhrom::Build(words, options);
    if (!image.IsOk()) {
      return Status(
          image.GetStatus().Code(),
          fmt::format("PHROM {}: {}", number, image.GetStatus().Message()));
    }
    images.push_back(*std::move(image));
  }
  return images;
}

StatusOr<std::vector<packaging::PhromImage>> BuildAcornPhroms(
    const Project& project) {
  std::vector<const Phrase*> phrases;
  phrases.reserve(project.Phrases().size());
  for (const Phrase& phrase : project.Phrases()) {
    phrases.push_back(&phrase);
  }
  return BuildAcornPhroms(project.Settings(), phrases);
}

StatusOr<std::vector<bbc::BbcDiscPhrom>> BuildBbcDiscPhroms(
    const ProjectSettings& settings, std::span<const Phrase* const> phrases) {
  StatusOr<std::vector<packaging::PhromImage>> images =
      BuildAcornPhroms(settings, phrases);
  if (!images.IsOk()) {
    return images.GetStatus();
  }
  if (images->size() > static_cast<std::size_t>(bbc::BbcDisc::kMaxPhroms)) {
    return ResourceExhaustedError(fmt::format(
        "these phrases fill {} PHROMs, and a BBC Micro disc holds at most {}: "
        "export fewer phrases",
        images->size(), bbc::BbcDisc::kMaxPhroms));
  }
  std::map<std::string, const Phrase*> by_name;
  for (const Phrase* phrase : phrases) {
    by_name.emplace(phrase->name, phrase);
  }
  std::vector<bbc::BbcDiscPhrom> phroms;
  phroms.reserve(images->size());
  for (std::size_t i = 0; i < images->size(); ++i) {
    const std::vector<std::string> strings =
        PhromHeaderStrings(settings, static_cast<int>(i) + 1);
    bbc::BbcDiscPhrom phrom{.title = strings.size() > 1 ? strings[1] : "",
                            .phrases = {}};
    for (const packaging::WordTableEntry& word : (*images)[i].table) {
      const auto found = by_name.find(word.name);
      if (found == by_name.end()) {
        return InternalError(word.name + " was placed but is not exported");
      }
      // A placed phrase always has frames.
      StatusOr<const std::vector<lpc::Frame>*> frames =
          FramesOf(*found->second);
      if (!frames.IsOk()) {
        return frames.GetStatus();
      }
      StatusOr<std::vector<std::uint8_t>> stream =
          packaging::SpeakExternalStream::Build(**frames);
      if (!stream.IsOk()) {
        return Status(
            stream.GetStatus().Code(),
            fmt::format("{}: {}", word.name, stream.GetStatus().Message()));
      }
      phrom.phrases.push_back(
          {.name = word.name, .stream = *std::move(stream)});
    }
    phroms.push_back(std::move(phrom));
  }
  return phroms;
}

StatusOr<std::vector<ExportedFile>> RunExport(const Project& project,
                                              const ExportPreset& preset,
                                              std::string_view base,
                                              const ExportTools& tools) {
  if (Status status = CheckExportBase(base); !status.IsOk()) {
    return status;
  }
  if (Status status = CheckExportPreset(preset); !status.IsOk()) {
    return status;
  }
  StatusOr<std::vector<const Phrase*>> phrases =
      PhrasesInScope(project, preset);
  if (!phrases.IsOk()) {
    return phrases.GetStatus();
  }
  StatusOr<std::vector<ExportedFile>> files = std::vector<ExportedFile>{};
  if (preset.format == kAcornPhromFormat) {
    files = AcornPhromFiles(project, preset, *phrases, base);
  } else if (preset.format == kCHeaderPhromFormat) {
    files = PhromHeaderFiles(project, preset, *phrases, base);
  } else if (preset.format == kCHeaderSpeakExternalFormat) {
    files = StreamHeaderFiles(project, preset, *phrases, base);
  } else if (preset.format == kBbcDiscFormat) {
    files = BbcDiscFiles(project, *phrases, base);
  } else {
    files = WavFiles(preset, *phrases, base, tools);
  }
  if (files.IsOk()) {
    Logger(kLogModuleExport)
        ->info("export \"{}\": {} phrase(s) as {} in {} file(s)", preset.name,
               phrases->size(), preset.format, files->size());
  }
  return files;
}

StatusOr<std::vector<std::string>> ExportFileNames(const Project& project,
                                                   const ExportPreset& preset,
                                                   std::string_view base) {
  if (Status status = CheckExportBase(base); !status.IsOk()) {
    return status;
  }
  if (Status status = CheckExportPreset(preset); !status.IsOk()) {
    return status;
  }
  StatusOr<std::vector<const Phrase*>> phrases =
      PhrasesInScope(project, preset);
  if (!phrases.IsOk()) {
    return phrases.GetStatus();
  }
  std::vector<std::string> names;
  if (preset.format == kBbcDiscFormat) {
    if (StatusOr<std::vector<bbc::BbcDiscPhrom>> phroms =
            BuildBbcDiscPhroms(project.Settings(), *phrases);
        !phroms.IsOk()) {
      return phroms.GetStatus();
    }
    names.push_back(fmt::format("{}.dsd", base));
    return names;
  }
  if (preset.format == kAcornPhromFormat ||
      preset.format == kCHeaderPhromFormat) {
    StatusOr<std::vector<packaging::PhromImage>> images =
        BuildAcornPhroms(project.Settings(), *phrases);
    if (!images.IsOk()) {
      return images.GetStatus();
    }
    if (preset.format == kCHeaderPhromFormat) {
      names.push_back(fmt::format("{}.h", base));
    } else if (preset.layout == kLayoutConcatenated) {
      names.push_back(fmt::format("{}.bin", base));
    } else {
      for (std::size_t i = 0; i < images->size(); ++i) {
        names.push_back(fmt::format("{}_{}.bin", base, i + 1));
      }
    }
    return names;
  }
  const bool original =
      preset.format == kWavFormat && preset.audio == kAudioOriginal;
  for (const Phrase* phrase : *phrases) {
    if (original) {
      if (phrase->audio.empty()) {
        return FailedPreconditionError(fmt::format(
            "{} has no source audio, so its original cannot be exported",
            phrase->name));
      }
    } else if (StatusOr<const std::vector<lpc::Frame>*> frames =
                   FramesOf(*phrase);
               !frames.IsOk()) {
      return frames.GetStatus();
    }
  }
  if (preset.format == kCHeaderSpeakExternalFormat) {
    names.push_back(fmt::format("{}.h", base));
    return names;
  }
  for (const std::string& identifier :
       CIdentifiersForNames(NamesOf(*phrases))) {
    names.push_back(fmt::format("{}_{}.wav", base, identifier));
  }
  return names;
}

Status CheckEncodingsCurrent(const Project& project,
                             const ExportPreset& preset) {
  if (preset.format == kWavFormat && preset.audio == kAudioOriginal) {
    return OkStatus();
  }
  StatusOr<std::vector<const Phrase*>> phrases =
      PhrasesInScope(project, preset);
  if (!phrases.IsOk()) {
    return phrases.GetStatus();
  }
  for (const Phrase* phrase : *phrases) {
    StatusOr<bool> current =
        EncodingCache::IsCurrent(*phrase, project.Settings().default_encoder);
    if (!current.IsOk()) {
      return current.GetStatus();
    }
    if (!*current) {
      return FailedPreconditionError(
          phrase->name + " is not encoded with its current settings");
    }
  }
  return OkStatus();
}

Status WriteExportedFiles(const std::filesystem::path& directory,
                          std::span<const ExportedFile> files) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    return UnavailableError(fmt::format("cannot create {}: {}",
                                        directory.string(), error.message()));
  }
  for (const ExportedFile& file : files) {
    const std::filesystem::path path = directory / file.name;
    std::ofstream stream(path, std::ios::binary | std::ios::trunc);
    const std::string bytes(file.bytes.begin(), file.bytes.end());
    stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    stream.close();
    if (!stream) {
      return UnavailableError("cannot write " + path.string());
    }
    Logger(kLogModuleExport)->debug("wrote {}", path.string());
  }
  return OkStatus();
}

}  // namespace speak5220::core::project
