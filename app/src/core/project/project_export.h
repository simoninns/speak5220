/*
 * File:        project_export.h
 * Module:      project
 * Purpose:     Runs a project's export presets: Acorn PHROM images, C
 *              headers, WAV files and the BBC Micro disc
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PROJECT_EXPORT_H_
#define SPEAK5220_CORE_PROJECT_PROJECT_EXPORT_H_

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "core/audio/wav_io.h"
#include "core/bbc/bbc_disc.h"
#include "core/packaging/phrom_image.h"
#include "core/project/project.h"
#include "core/synth/synth.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// Export formats and their options.
inline constexpr std::string_view kAcornPhromFormat = "acorn-phrom";
inline constexpr std::string_view kCHeaderPhromFormat = "c-header-phrom";
inline constexpr std::string_view kCHeaderSpeakExternalFormat =
    "c-header-speak-external";
inline constexpr std::string_view kWavFormat = "wav";
inline constexpr std::string_view kBbcDiscFormat = "bbc-dsd";
inline constexpr std::string_view kScopeAll = "all";
inline constexpr std::string_view kScopeSelected = "selected";
inline constexpr std::string_view kLayoutSeparate = "separate";
inline constexpr std::string_view kLayoutConcatenated = "concatenated";
inline constexpr std::string_view kAudioEncoded = "encoded";
inline constexpr std::string_view kAudioOriginal = "original";
inline constexpr std::string_view kAudioPreprocessed = "preprocessed";
// The licence of a generated C header when the preset names none.
inline constexpr std::string_view kDefaultCHeaderLicence = "GPL-3.0-or-later";

// What a format is called, and which preset options it takes.
struct ExportFormatInfo {
  std::string_view key;
  // The name in the Export dialog.
  std::string_view label;
  bool takes_layout = false;
  bool takes_licence = false;
  bool takes_audio = false;
};

// Every format, in the order the Export dialog lists them.
std::span<const ExportFormatInfo> ExportFormats();
// The format with `key`, or null for an unknown one.
const ExportFormatInfo* FindExportFormat(std::string_view key);

// What the WAV export needs: the chip, to speak the encoded phrases, and
// a WAV encoder. Not owned; must outlive the call. The other formats need
// neither.
struct ExportTools {
  synth::ISynth* synth = nullptr;
  audio::IWavCodec* wav_codec = nullptr;
};

// A file an export produces: its name (no directory) and its bytes.
struct ExportedFile {
  std::string name = {};
  std::vector<std::uint8_t> bytes = {};

  friend bool operator==(const ExportedFile& a,
                         const ExportedFile& b) = default;
};

// Checks a preset's own fields: a known format, scope "all" (or empty) or
// "selected" with at least one phrase, and the options its format takes
// with values it accepts (a layout, a licence, an audio choice). An option
// its format does not take must be empty. The phrases are not looked up.
Status CheckExportPreset(const ExportPreset& preset);

// Checks the base that exported files are named from (a preset's name, or
// the project file's): one file name, so every file lands in the chosen
// directory. Refuses an empty base, "." and "..", and the characters
// / \ : * ? " < > | and control characters, which some systems do not
// allow in a name.
Status CheckExportBase(std::string_view base);

// The phrases `preset` covers, in list order: all of them, or for scope
// "selected" those it names. Fails for a name no phrase has.
StatusOr<std::vector<const Phrase*>> PhrasesInScope(const Project& project,
                                                    const ExportPreset& preset);

// The header strings of PHROM `number` (from 1): its own, if
// the project sets them, or the project's with " <number>" appended to the
// second string.
std::vector<std::string> PhromHeaderStrings(const ProjectSettings& settings,
                                            int number);

// One Acorn PHROM image per PHROM that `phrases` fill, allocated in their
// order as the project's are (speak5220 datasheet §12.8), from
// their current frames: word numbers from 127, each PHROM with the ASCII
// entries whose phrase it holds. Fails if a phrase has no frames or is too
// large for a PHROM.
StatusOr<std::vector<packaging::PhromImage>> BuildAcornPhroms(
    const ProjectSettings& settings, std::span<const Phrase* const> phrases);
// The same for every phrase of `project`.
StatusOr<std::vector<packaging::PhromImage>> BuildAcornPhroms(
    const Project& project);

// The PHROMs of a BBC Micro disc for `phrases`: allocated as
// BuildAcornPhroms() allocates them, each with the second of its header
// strings as its title and its phrases' Speak External streams
// (speak5220 datasheet §12.9). Fails as BuildAcornPhroms(), and if the
// phrases fill more PHROMs than the disc holds (bbc::BbcDisc::kMaxPhroms).
StatusOr<std::vector<bbc::BbcDiscPhrom>> BuildBbcDiscPhroms(
    const ProjectSettings& settings, std::span<const Phrase* const> phrases);

// Runs `preset` (CheckExportPreset()) over the phrases in its scope. Files
// are named from `base`:
// - acorn-phrom: "<base>_1.bin", "<base>_2.bin", ... for separate images,
//   or "<base>.bin" for concatenated ones;
// - the C headers: "<base>.h", with names prefixed by the C identifier of
//   `base` in lower case (constants in upper case);
// - wav: "<base>_<identifier>.wav" per phrase, with the phrase's C
//   identifier. "encoded" is the chip's SPEAKER output through
//   the VSM path, 16-bit at 8 kHz; "original" the phrase's 8 kHz source,
//   24-bit, as the project holds it; "preprocessed" that source
//   preprocessed by the phrase's options (preprocess::Preprocess()),
//   24-bit, clipped at full scale.
// - bbc-dsd: "<base>.dsd", the BBC Micro disc (BuildBbcDiscPhroms(),
//   bbc::BbcDisc), titled with the project's name.
// Fails as CheckExportBase(), CheckExportPreset() and PhrasesInScope(), for a
// phrase with no frames (or no source audio, for "original" and
// "preprocessed"), as
// BuildAcornPhroms() and BuildBbcDiscPhroms(), and for wav without `tools`.
// Speaking phrases takes milliseconds per second of speech, so a WAV export is
// not run on the GUI thread. Thread-safe if the tools are.
StatusOr<std::vector<ExportedFile>> RunExport(const Project& project,
                                              const ExportPreset& preset,
                                              std::string_view base,
                                              const ExportTools& tools = {});

// The names of the files RunExport() would write, after the same checks,
// without speaking any phrase or writing a header: what the Export dialog
// shows before the user exports. Fails as RunExport() would, except for
// failures of the chip or the WAV encoder.
StatusOr<std::vector<std::string>> ExportFileNames(const Project& project,
                                                   const ExportPreset& preset,
                                                   std::string_view base);

// Fails, naming the phrase, unless every phrase `preset` exports is
// encoded with its current settings (EncodingCache::IsCurrent()), as an
// export from a stale encoding would not match the project. A WAV export
// of the original audio needs no encoding.
Status CheckEncodingsCurrent(const Project& project,
                             const ExportPreset& preset);

// Writes `files` into `directory`, creating it if needed and replacing
// files of the same names. Fails, naming the path, if a file cannot be
// written; files written before it are left.
Status WriteExportedFiles(const std::filesystem::path& directory,
                          std::span<const ExportedFile> files);

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PROJECT_EXPORT_H_
