/*
 * File:        project_serializer.h
 * Module:      project
 * Purpose:     Converts a project to and from its files: project.yaml, and
 *              each phrase's source.wav and frames.yaml
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_PROJECT_PROJECT_SERIALIZER_H_
#define SPEAK5220_CORE_PROJECT_PROJECT_SERIALIZER_H_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "core/audio/wav_io.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_store.h"
#include "core/util/status.h"

namespace speak5220::core::project {

// The files of a project (plan §4).
inline constexpr std::string_view kProjectYamlPath = "project.yaml";
inline constexpr std::string_view kPhrasesDirectory = "phrases";
inline constexpr std::string_view kSourceWavName = "source.wav";
inline constexpr std::string_view kFramesYamlName = "frames.yaml";

// A phrase's frames.yaml (plan §4): the encoder version, the settings
// hash, the frames, one flow mapping each, and the cached quality. Pure.
std::string FramesYaml(const CachedEncoding& encoding);

// Reads a frames.yaml; `file` names it in messages. Keys other than
// "frames" are optional. Fails for a missing or invalid frame. Pure.
StatusOr<CachedEncoding> ReadFramesYaml(const std::vector<std::uint8_t>& bytes,
                                        const std::string& file);

// Plan §4: the project's metadata as YAML 1.2 (yaml-cpp), with snake_case
// keys named after the C++ fields; each phrase's audio as 8 kHz mono
// 24-bit PCM through an injected IWavCodec; its frames as YAML.
//
// Writing: strings are always quoted, so names such as NO and ON stay
// strings for any YAML reader; numbers are written in the C locale; keys
// this version does not know, and files it does not use, are written back
// as they were read.
//
// Reading: "format" and "version" are checked and older versions migrated
// (ProjectMigrationSteps()); every known value is type-checked and range
// checked, with a message naming the file and the key; the result must
// keep Project's rules (Project::FromContents()).
//
// Thread-compatible; the codec must outlive the serializer.
class ProjectSerializer {
 public:
  explicit ProjectSerializer(audio::IWavCodec& codec);

  StatusOr<ProjectFiles> ToFiles(const Project& project);
  StatusOr<Project> FromFiles(const ProjectFiles& files);

  // ToFiles() into `store`, and `store`'s files through FromFiles().
  Status Save(const Project& project, IProjectStore& store);
  StatusOr<Project> Load(IProjectStore& store);

 private:
  audio::IWavCodec* codec_;
};

}  // namespace speak5220::core::project

#endif  // SPEAK5220_CORE_PROJECT_PROJECT_SERIALIZER_H_
