/*
 * File:        project_serializer.cpp
 * Module:      project
 * Purpose:     Converts a project to and from its files: project.yaml, and
 *              each phrase's source.wav and frames.yaml
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/project_serializer.h"

#include <spdlog/fmt/fmt.h>
#include <yaml-cpp/yaml.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/audio/pcm24.h"
#include "core/audio/wav_io.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_migration.h"
#include "core/quality/quality_assessor.h"
#include "core/util/logging.h"
#include "core/util/number_text.h"

namespace speak5220::core::project {
namespace {

using encoder::EncoderOptionInfo;
using encoder::EncoderOptionType;

// yaml-cpp's tags for an untagged scalar: "?" when it was plain, "!" when
// it was quoted.
constexpr std::string_view kPlainTag = "?";
constexpr std::string_view kQuotedTag = "!";

// ---------------------------------------------------------------------
// Writing
// ---------------------------------------------------------------------

void EmitQuoted(YAML::Emitter& out, std::string_view text) {
  out << YAML::DoubleQuoted << std::string(text);
}

void EmitInteger(YAML::Emitter& out, std::int64_t value) {
  // Written as text: yaml-cpp's own integer output goes through a stream
  // in the global locale, which could group digits.
  out << std::to_string(value);
}

void EmitNumber(YAML::Emitter& out, double value) {
  out << FormatNumber(value);
}

void EmitBool(YAML::Emitter& out, bool value) { out << value; }

void EmitStringList(YAML::Emitter& out, const std::vector<std::string>& list) {
  out << YAML::Flow << YAML::BeginSeq;
  for (const std::string& text : list) {
    EmitQuoted(out, text);
  }
  out << YAML::EndSeq;
}

// Writes a node read from a file as it was: quoted scalars stay quoted,
// plain ones plain, and flow collections flow. yaml-cpp's own output of a
// node loses the quoting.
void EmitNode(YAML::Emitter& out, const YAML::Node& node) {
  switch (node.Type()) {
    case YAML::NodeType::Scalar:
      if (node.Tag() == kQuotedTag) {
        EmitQuoted(out, node.Scalar());
      } else if (node.Tag() == kPlainTag || node.Tag().empty()) {
        out << node.Scalar();
      } else {
        out << YAML::VerbatimTag(node.Tag()) << node.Scalar();
      }
      return;
    case YAML::NodeType::Sequence:
      if (node.Style() == YAML::EmitterStyle::Flow) {
        out << YAML::Flow;
      }
      out << YAML::BeginSeq;
      for (const YAML::Node& child : node) {
        EmitNode(out, child);
      }
      out << YAML::EndSeq;
      return;
    case YAML::NodeType::Map:
      if (node.Style() == YAML::EmitterStyle::Flow) {
        out << YAML::Flow;
      }
      out << YAML::BeginMap;
      for (const auto& entry : node) {
        out << YAML::Key;
        EmitNode(out, entry.first);
        out << YAML::Value;
        EmitNode(out, entry.second);
      }
      out << YAML::EndMap;
      return;
    case YAML::NodeType::Null:
    case YAML::NodeType::Undefined:
      out << YAML::Null;
      return;
  }
}

std::string NodeText(const YAML::Node& node) {
  YAML::Emitter out;
  out.SetNullFormat(YAML::LowerNull);
  EmitNode(out, node);
  return out.c_str();
}

void EmitUnknown(YAML::Emitter& out, const UnknownKeys& unknown) {
  for (const auto& [key, text] : unknown) {
    out << YAML::Key << key << YAML::Value;
    EmitNode(out, YAML::Load(text));
  }
}

// One option's value, typed as the option is (plan §4).
void EmitOptionValue(YAML::Emitter& out, const EncoderOptionInfo& info,
                     const std::string& text) {
  switch (info.type) {
    case EncoderOptionType::kOptionalReal:
      if (text.empty() || text == encoder::kUnsetOptionText) {
        out << YAML::Null;
        return;
      }
      out << text;
      return;
    case EncoderOptionType::kPreEmphasis:
      if (text == "chirp") {
        EmitQuoted(out, text);
        return;
      }
      out << text;
      return;
    case EncoderOptionType::kReal:
    case EncoderOptionType::kInteger:
      out << text;
      return;
  }
}

void BeginDocument(YAML::Emitter& out) {
  out.SetIndent(2);
  out.SetNullFormat(YAML::LowerNull);
  out.SetBoolFormat(YAML::TrueFalseBool);
  out << YAML::BeginMap;
}

void EmitSettings(YAML::Emitter& out, const ProjectSettings& settings) {
  out << YAML::Key << "project" << YAML::Value << YAML::BeginMap;
  out << YAML::Key << "name" << YAML::Value;
  EmitQuoted(out, settings.name);
  out << YAML::Key << "header_strings" << YAML::Value;
  EmitStringList(out, settings.header_strings);
  out << YAML::Key << "ascii_map" << YAML::Value;
  if (settings.ascii_map.empty()) {
    out << YAML::Flow;
  }
  out << YAML::BeginMap;
  for (const auto& [character, name] : settings.ascii_map) {
    out << YAML::Key;
    EmitQuoted(out, std::string(1, character));
    out << YAML::Value;
    EmitQuoted(out, name);
  }
  out << YAML::EndMap;
  // Every option, so the project never depends on the defaults.
  out << YAML::Key << "default_encoder" << YAML::Value << YAML::BeginMap;
  for (const EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    out << YAML::Key << std::string(info.field) << YAML::Value;
    EmitOptionValue(
        out, info,
        encoder::EncoderOptionText(settings.default_encoder, info.field));
  }
  EmitUnknown(out, settings.default_encoder_unknown);
  out << YAML::EndMap;
  if (!settings.phroms.empty()) {
    out << YAML::Key << "phroms" << YAML::Value << YAML::BeginSeq;
    for (const PhromHeader& phrom : settings.phroms) {
      out << YAML::BeginMap;
      out << YAML::Key << "number" << YAML::Value;
      EmitInteger(out, phrom.number);
      out << YAML::Key << "header_strings" << YAML::Value;
      EmitStringList(out, phrom.header_strings);
      EmitUnknown(out, phrom.unknown);
      out << YAML::EndMap;
    }
    out << YAML::EndSeq;
  }
  EmitUnknown(out, settings.unknown);
  out << YAML::EndMap;
}

void EmitPhrase(YAML::Emitter& out, const Phrase& phrase) {
  out << YAML::BeginMap;
  out << YAML::Key << "id" << YAML::Value;
  EmitQuoted(out, phrase.id);
  out << YAML::Key << "name" << YAML::Value;
  EmitQuoted(out, phrase.name);
  out << YAML::Key << "description" << YAML::Value;
  EmitQuoted(out, phrase.description);
  if (phrase.source.has_value()) {
    const SourceInfo& source = *phrase.source;
    out << YAML::Key << "source" << YAML::Value << YAML::BeginMap;
    out << YAML::Key << "file" << YAML::Value;
    EmitQuoted(out, source.file);
    out << YAML::Key << "rate" << YAML::Value;
    EmitInteger(out, source.rate);
    out << YAML::Key << "channels" << YAML::Value;
    EmitInteger(out, source.channels);
    out << YAML::Key << "duration" << YAML::Value;
    EmitNumber(out, source.duration);
    out << YAML::Key << "sha256" << YAML::Value;
    EmitQuoted(out, source.sha256);
    EmitUnknown(out, source.unknown);
    out << YAML::EndMap;
  }
  if (!phrase.encoder.empty() || !phrase.encoder_unknown.empty()) {
    out << YAML::Key << "encoder" << YAML::Value << YAML::BeginMap;
    // In the order of EncoderOptionInfos(), as in default_encoder.
    for (const EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
      const auto found = phrase.encoder.find(std::string(info.field));
      if (found != phrase.encoder.end()) {
        out << YAML::Key << std::string(info.field) << YAML::Value;
        EmitOptionValue(out, info, found->second);
      }
    }
    EmitUnknown(out, phrase.encoder_unknown);
    out << YAML::EndMap;
  }
  out << YAML::Key << "locked" << YAML::Value;
  EmitBool(out, phrase.locked);
  out << YAML::Key << "play_start" << YAML::Value;
  EmitInteger(out, phrase.play_start);
  EmitUnknown(out, phrase.unknown);
  out << YAML::EndMap;
}

void EmitExport(YAML::Emitter& out, const ExportPreset& preset) {
  out << YAML::BeginMap;
  out << YAML::Key << "name" << YAML::Value;
  EmitQuoted(out, preset.name);
  out << YAML::Key << "format" << YAML::Value;
  EmitQuoted(out, preset.format);
  out << YAML::Key << "scope" << YAML::Value;
  EmitQuoted(out, preset.scope);
  if (!preset.layout.empty()) {
    out << YAML::Key << "layout" << YAML::Value;
    EmitQuoted(out, preset.layout);
  }
  EmitUnknown(out, preset.unknown);
  out << YAML::EndMap;
}

std::string ProjectYaml(const ProjectContents& contents) {
  YAML::Emitter out;
  BeginDocument(out);
  out << YAML::Key << "format" << YAML::Value;
  EmitQuoted(out, kProjectFormatName);
  out << YAML::Key << "version" << YAML::Value;
  EmitInteger(out, kProjectFormatVersion);
  EmitSettings(out, contents.settings);
  out << YAML::Key << "phrases" << YAML::Value;
  if (contents.phrases.empty()) {
    out << YAML::Flow;
  }
  out << YAML::BeginSeq;
  for (const Phrase& phrase : contents.phrases) {
    EmitPhrase(out, phrase);
  }
  out << YAML::EndSeq;
  out << YAML::Key << "exports" << YAML::Value;
  if (contents.exports.empty()) {
    out << YAML::Flow;
  }
  out << YAML::BeginSeq;
  for (const ExportPreset& preset : contents.exports) {
    EmitExport(out, preset);
  }
  out << YAML::EndSeq;
  EmitUnknown(out, contents.unknown);
  out << YAML::EndMap;
  return std::string(out.c_str()) + "\n";
}

// A frame as one flow mapping (plan §4), with only the fields its kind
// carries.
void EmitFrame(YAML::Emitter& out, const lpc::Frame& frame) {
  out << YAML::Flow << YAML::BeginMap;
  out << YAML::Key << "energy" << YAML::Value;
  EmitInteger(out, frame.energy);
  const lpc::FrameType type = lpc::FrameKind(frame);
  if (type == lpc::FrameType::kRepeat || type == lpc::FrameType::kUnvoiced ||
      type == lpc::FrameType::kVoiced) {
    out << YAML::Key << "repeat" << YAML::Value;
    EmitInteger(out, frame.repeat ? 1 : 0);
    out << YAML::Key << "pitch" << YAML::Value;
    EmitInteger(out, frame.pitch);
  }
  const int k_count = lpc::FrameKCount(frame);
  if (k_count > 0) {
    out << YAML::Key << "k" << YAML::Value << YAML::Flow << YAML::BeginSeq;
    for (int i = 0; i < k_count; ++i) {
      EmitInteger(out, frame.k.at(static_cast<std::size_t>(i)));
    }
    out << YAML::EndSeq;
  }
  out << YAML::EndMap;
}

// The figures of a quality mapping, by key, in the order they are written
// (bar measures_version and estoi).
constexpr std::array<std::pair<std::string_view, double CachedQuality::*>, 7>
    kQualityFigures = {{
        {"lsd_db", &CachedQuality::lsd_db},
        {"lsd_over_2db", &CachedQuality::lsd_over_2db},
        {"lsd_over_4db", &CachedQuality::lsd_over_4db},
        {"gross_pitch_error", &CachedQuality::gross_pitch_error},
        {"f0_rmse_cents", &CachedQuality::f0_rmse_cents},
        {"voicing_error", &CachedQuality::voicing_error},
        {"score", &CachedQuality::score},
    }};

std::string FramesYaml(const CachedEncoding& encoding) {
  YAML::Emitter out;
  BeginDocument(out);
  out << YAML::Key << "encoder_version" << YAML::Value;
  EmitQuoted(out, encoding.encoder_version);
  out << YAML::Key << "settings_hash" << YAML::Value;
  EmitQuoted(out, encoding.settings_hash);
  out << YAML::Key << "frames" << YAML::Value;
  if (encoding.frames.empty()) {
    out << YAML::Flow;
  }
  out << YAML::BeginSeq;
  for (const lpc::Frame& frame : encoding.frames) {
    EmitFrame(out, frame);
  }
  out << YAML::EndSeq;
  if (encoding.quality || !encoding.quality_unknown.empty()) {
    out << YAML::Key << "quality" << YAML::Value << YAML::BeginMap;
    if (const std::optional<CachedQuality>& q = encoding.quality) {
      out << YAML::Key << "measures_version" << YAML::Value;
      EmitInteger(out, q->measures_version);
      for (const auto& [key, member] : kQualityFigures) {
        if (key == "score" && q->estoi) {
          out << YAML::Key << "estoi" << YAML::Value;
          EmitNumber(out, *q->estoi);
        }
        out << YAML::Key << std::string(key) << YAML::Value;
        EmitNumber(out, (*q).*member);
      }
    }
    EmitUnknown(out, encoding.quality_unknown);
    out << YAML::EndMap;
  }
  EmitUnknown(out, encoding.unknown);
  out << YAML::EndMap;
  return std::string(out.c_str()) + "\n";
}

std::vector<std::uint8_t> Bytes(std::string_view text) {
  return {text.begin(), text.end()};
}

std::string PhraseDirectory(const PhraseId& id) {
  return fmt::format("{}/{}/", kPhrasesDirectory, id);
}

// ---------------------------------------------------------------------
// Reading
// ---------------------------------------------------------------------

// Where a value is, for messages: "project.yaml: phrases[2].name".
class Where {
 public:
  Where(std::string_view file, std::string path)
      : file_(file), path_(std::move(path)) {}

  Where Key(std::string_view key) const {
    return {file_, path_.empty() ? std::string(key)
                                 : fmt::format("{}.{}", path_, key)};
  }
  Where Index(std::size_t index) const {
    return {file_, fmt::format("{}[{}]", path_, index)};
  }

  Status Error(std::string_view problem) const {
    return DataLossError(path_.empty()
                             ? fmt::format("{}: {}", file_, problem)
                             : fmt::format("{}: {} {}", file_, path_, problem));
  }

 private:
  std::string_view file_;
  std::string path_;
};

bool IsPlain(const YAML::Node& node) { return node.Tag() == kPlainTag; }

StatusOr<YAML::Node> ParseYaml(const std::vector<std::uint8_t>& bytes,
                               std::string_view file) {
  const std::string text(bytes.begin(), bytes.end());
  try {
    return YAML::Load(text);
  } catch (const YAML::Exception& error) {
    return DataLossError(
        fmt::format("{} is not valid YAML: {}", file, error.what()));
  }
}

Status ExpectMap(const YAML::Node& node, const Where& where) {
  return node.IsMap() ? OkStatus() : where.Error("must be a mapping");
}

StatusOr<std::string> ReadString(const YAML::Node& node, const Where& where) {
  if (!node.IsScalar()) {
    return where.Error("must be a string");
  }
  return node.Scalar();
}

StatusOr<double> ReadNumber(const YAML::Node& node, const Where& where) {
  if (!node.IsScalar() || !IsPlain(node)) {
    return where.Error("must be a number");
  }
  const std::optional<double> value = ParseNumber(node.Scalar());
  if (!value) {
    return where.Error("must be a number");
  }
  return *value;
}

StatusOr<int> ReadInteger(const YAML::Node& node, const Where& where,
                          std::int64_t minimum, std::int64_t maximum) {
  if (!node.IsScalar() || !IsPlain(node)) {
    return where.Error("must be a whole number");
  }
  const std::optional<std::int64_t> value = ParseInteger(node.Scalar());
  if (!value) {
    return where.Error("must be a whole number");
  }
  if (*value < minimum || *value > maximum) {
    return where.Error(fmt::format("must be from {} to {}", minimum, maximum));
  }
  return static_cast<int>(*value);
}

StatusOr<bool> ReadBool(const YAML::Node& node, const Where& where) {
  if (node.IsScalar() && IsPlain(node)) {
    if (node.Scalar() == "true") {
      return true;
    }
    if (node.Scalar() == "false") {
      return false;
    }
  }
  return where.Error("must be true or false");
}

StatusOr<std::vector<std::string>> ReadStringList(const YAML::Node& node,
                                                  const Where& where) {
  if (!node.IsSequence()) {
    return where.Error("must be a list of strings");
  }
  std::vector<std::string> list;
  for (std::size_t i = 0; i < node.size(); ++i) {
    StatusOr<std::string> text = ReadString(node[i], where.Index(i));
    if (!text.IsOk()) {
      return text.GetStatus();
    }
    list.push_back(*std::move(text));
  }
  return list;
}

constexpr std::int64_t kIntMax = std::numeric_limits<int>::max();

// Calls `read(key, value)` for each entry of a mapping, refusing a key
// that is not a scalar or is repeated.
template <typename ReadEntry>
Status ForEachEntry(const YAML::Node& node, const Where& where,
                    ReadEntry read) {
  if (Status status = ExpectMap(node, where); !status.IsOk()) {
    return status;
  }
  std::set<std::string> seen;
  for (const auto& entry : node) {
    if (!entry.first.IsScalar()) {
      return where.Error("has a key that is not a string");
    }
    const std::string key = entry.first.Scalar();
    if (!seen.insert(key).second) {
      return where.Key(key).Error("is given twice");
    }
    if (Status status = read(key, entry.second); !status.IsOk()) {
      return status;
    }
  }
  return OkStatus();
}

// An option value as SetEncoderOption() reads it, checked against the
// option's YAML type (plan §4).
StatusOr<std::string> ReadOptionText(const YAML::Node& node,
                                     const EncoderOptionInfo& info,
                                     const Where& where) {
  if (node.IsNull() && info.type == EncoderOptionType::kOptionalReal) {
    return std::string(encoder::kUnsetOptionText);
  }
  if (!node.IsScalar()) {
    return where.Error("must be a single value");
  }
  const bool quoted_chirp =
      info.type == EncoderOptionType::kPreEmphasis && node.Scalar() == "chirp";
  if (!IsPlain(node) && !quoted_chirp) {
    return where.Error("must not be quoted: it is a number or true/false");
  }
  return node.Scalar();
}

// Reads an encoder mapping: known options through `apply`, other keys kept.
template <typename Apply>
Status ReadEncoderMapping(const YAML::Node& node, const Where& where,
                          UnknownKeys& unknown, Apply apply) {
  return ForEachEntry(
      node, where, [&](const std::string& key, const YAML::Node& value) {
        const EncoderOptionInfo* info = encoder::FindEncoderOption(key);
        if (info == nullptr) {
          unknown.emplace_back(key, NodeText(value));
          return OkStatus();
        }
        StatusOr<std::string> text =
            ReadOptionText(value, *info, where.Key(key));
        if (!text.IsOk()) {
          return text.GetStatus();
        }
        if (Status status = apply(key, *text); !status.IsOk()) {
          return where.Key(key).Error(status.Message());
        }
        return OkStatus();
      });
}

// Fails unless project.default_encoder lists every encoder option (plan
// §4), as projects saved before the defaults last changed do not.
Status CheckEveryOptionListed(const YAML::Node& root, const Where& where) {
  const Where at = where.Key("project").Key("default_encoder");
  const YAML::Node settings = root["project"];
  const YAML::Node options = settings.IsDefined() && settings.IsMap()
                                 ? settings["default_encoder"]
                                 : YAML::Node();
  if (!options.IsDefined() || !options.IsMap()) {
    return at.Error("is missing: a project lists every encoder option");
  }
  std::string missing;
  for (const EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    if (!options[std::string(info.field)].IsDefined()) {
      missing += missing.empty() ? "" : ", ";
      missing += info.field;
    }
  }
  if (!missing.empty()) {
    return at.Error(fmt::format(
        "does not list {}: a project lists every encoder option, so this "
        "one was saved by an earlier development version of Speak5220 and "
        "cannot be opened",
        missing));
  }
  return OkStatus();
}

Status ReadSettings(const YAML::Node& node, const Where& where,
                    ProjectSettings& settings) {
  return ForEachEntry(
      node, where,
      [&](const std::string& key, const YAML::Node& value) -> Status {
        const Where at = where.Key(key);
        if (key == "name") {
          StatusOr<std::string> name = ReadString(value, at);
          if (!name.IsOk()) {
            return name.GetStatus();
          }
          settings.name = *std::move(name);
        } else if (key == "header_strings") {
          StatusOr<std::vector<std::string>> strings =
              ReadStringList(value, at);
          if (!strings.IsOk()) {
            return strings.GetStatus();
          }
          settings.header_strings = *std::move(strings);
        } else if (key == "ascii_map") {
          return ForEachEntry(
              value, at,
              [&](const std::string& character, const YAML::Node& name) {
                if (character.size() != 1) {
                  return at.Error(fmt::format(
                      "has the key \"{}\"; each key is one character",
                      character));
                }
                StatusOr<std::string> word =
                    ReadString(name, at.Key(character));
                if (!word.IsOk()) {
                  return word.GetStatus();
                }
                settings.ascii_map[character.front()] = *std::move(word);
                return OkStatus();
              });
        } else if (key == "default_encoder") {
          return ReadEncoderMapping(
              value, at, settings.default_encoder_unknown,
              [&](const std::string& field, const std::string& text) {
                return encoder::SetEncoderOption(settings.default_encoder,
                                                 field, text);
              });
        } else if (key == "phroms") {
          if (!value.IsSequence()) {
            return at.Error("must be a list");
          }
          for (std::size_t i = 0; i < value.size(); ++i) {
            PhromHeader phrom;
            const Where item = at.Index(i);
            if (Status status = ForEachEntry(
                    value[i], item,
                    [&](const std::string& field,
                        const YAML::Node& entry) -> Status {
                      if (field == "number") {
                        StatusOr<int> number =
                            ReadInteger(entry, item.Key(field), 1, kIntMax);
                        if (!number.IsOk()) {
                          return number.GetStatus();
                        }
                        phrom.number = *number;
                      } else if (field == "header_strings") {
                        StatusOr<std::vector<std::string>> strings =
                            ReadStringList(entry, item.Key(field));
                        if (!strings.IsOk()) {
                          return strings.GetStatus();
                        }
                        phrom.header_strings = *std::move(strings);
                      } else {
                        phrom.unknown.emplace_back(field, NodeText(entry));
                      }
                      return OkStatus();
                    });
                !status.IsOk()) {
              return status;
            }
            settings.phroms.push_back(std::move(phrom));
          }
        } else {
          settings.unknown.emplace_back(key, NodeText(value));
        }
        return OkStatus();
      });
}

Status ReadSource(const YAML::Node& node, const Where& where,
                  SourceInfo& source) {
  return ForEachEntry(
      node, where,
      [&](const std::string& key, const YAML::Node& value) -> Status {
        const Where at = where.Key(key);
        if (key == "file" || key == "sha256") {
          StatusOr<std::string> text = ReadString(value, at);
          if (!text.IsOk()) {
            return text.GetStatus();
          }
          (key == "file" ? source.file : source.sha256) = *std::move(text);
        } else if (key == "rate" || key == "channels") {
          // 0 stands for not known, as in a source written by hand.
          StatusOr<int> number = ReadInteger(value, at, 0, kIntMax);
          if (!number.IsOk()) {
            return number.GetStatus();
          }
          (key == "rate" ? source.rate : source.channels) = *number;
        } else if (key == "duration") {
          StatusOr<double> duration = ReadNumber(value, at);
          if (!duration.IsOk()) {
            return duration.GetStatus();
          }
          if (*duration < 0.0) {
            return at.Error("must not be negative");
          }
          source.duration = *duration;
        } else {
          source.unknown.emplace_back(key, NodeText(value));
        }
        return OkStatus();
      });
}

Status ReadPhrase(const YAML::Node& node, const Where& where, Phrase& phrase) {
  bool has_id = false;
  bool has_name = false;
  if (Status status = ForEachEntry(
          node, where,
          [&](const std::string& key, const YAML::Node& value) -> Status {
            const Where at = where.Key(key);
            if (key == "id" || key == "name" || key == "description") {
              StatusOr<std::string> text = ReadString(value, at);
              if (!text.IsOk()) {
                return text.GetStatus();
              }
              if (key == "id") {
                phrase.id = *std::move(text);
                has_id = true;
              } else if (key == "name") {
                phrase.name = *std::move(text);
                has_name = true;
              } else {
                phrase.description = *std::move(text);
              }
            } else if (key == "source") {
              SourceInfo source;
              if (Status source_status = ReadSource(value, at, source);
                  !source_status.IsOk()) {
                return source_status;
              }
              phrase.source = std::move(source);
            } else if (key == "encoder") {
              return ReadEncoderMapping(
                  value, at, phrase.encoder_unknown,
                  [&](const std::string& field, const std::string& text) {
                    // Checked on its own here; Project checks it against
                    // the defaults.
                    encoder::EncoderOptions scratch;
                    if (Status option_status =
                            encoder::SetEncoderOption(scratch, field, text);
                        !option_status.IsOk()) {
                      return option_status;
                    }
                    phrase.encoder[field] = text;
                    return OkStatus();
                  });
            } else if (key == "locked") {
              StatusOr<bool> locked = ReadBool(value, at);
              if (!locked.IsOk()) {
                return locked.GetStatus();
              }
              phrase.locked = *locked;
            } else if (key == "play_start") {
              StatusOr<int> start = ReadInteger(value, at, 0, kIntMax);
              if (!start.IsOk()) {
                return start.GetStatus();
              }
              phrase.play_start = *start;
            } else {
              phrase.unknown.emplace_back(key, NodeText(value));
            }
            return OkStatus();
          });
      !status.IsOk()) {
    return status;
  }
  if (!has_id) {
    return where.Error("has no id");
  }
  if (!has_name) {
    return where.Error("has no name");
  }
  if (!IsValidPhraseId(phrase.id)) {
    return where.Key("id").Error("must be a lower-case UUID");
  }
  return OkStatus();
}

Status ReadExport(const YAML::Node& node, const Where& where,
                  ExportPreset& preset) {
  return ForEachEntry(
      node, where,
      [&](const std::string& key, const YAML::Node& value) -> Status {
        std::string* field = nullptr;
        if (key == "name") {
          field = &preset.name;
        } else if (key == "format") {
          field = &preset.format;
        } else if (key == "scope") {
          field = &preset.scope;
        } else if (key == "layout") {
          field = &preset.layout;
        }
        if (field == nullptr) {
          preset.unknown.emplace_back(key, NodeText(value));
          return OkStatus();
        }
        StatusOr<std::string> text = ReadString(value, where.Key(key));
        if (!text.IsOk()) {
          return text.GetStatus();
        }
        *field = *std::move(text);
        return OkStatus();
      });
}

Status ReadProjectYaml(YAML::Node root, ProjectContents& contents) {
  const Where where(kProjectYamlPath, "");
  if (Status status = ExpectMap(root, where); !status.IsOk()) {
    return status;
  }
  const YAML::Node format = root["format"];
  if (!format.IsDefined() || !format.IsScalar() ||
      format.Scalar() != kProjectFormatName) {
    return where.Error(
        fmt::format("is not a Speak5220 project (its format is not \"{}\")",
                    kProjectFormatName));
  }
  StatusOr<int> version =
      ReadInteger(root["version"], where.Key("version"), 1, kIntMax);
  if (!version.IsOk()) {
    return version.GetStatus();
  }
  if (Status status =
          MigrateProjectDocument(root, *version, ProjectMigrationSteps());
      !status.IsOk()) {
    return status;
  }
  if (Status read = ForEachEntry(
          root, where,
          [&](const std::string& key, const YAML::Node& value) -> Status {
            const Where at = where.Key(key);
            if (key == "format" || key == "version") {
              return OkStatus();
            }
            if (key == "project") {
              return ReadSettings(value, at, contents.settings);
            }
            if (key == "phrases" || key == "exports") {
              if (!value.IsSequence()) {
                return at.Error("must be a list");
              }
              for (std::size_t i = 0; i < value.size(); ++i) {
                if (key == "phrases") {
                  Phrase phrase;
                  if (Status status = ReadPhrase(value[i], at.Index(i), phrase);
                      !status.IsOk()) {
                    return status;
                  }
                  contents.phrases.push_back(std::move(phrase));
                } else {
                  ExportPreset preset;
                  if (Status status = ReadExport(value[i], at.Index(i), preset);
                      !status.IsOk()) {
                    return status;
                  }
                  contents.exports.push_back(std::move(preset));
                }
              }
              return OkStatus();
            }
            contents.unknown.emplace_back(key, NodeText(value));
            return OkStatus();
          });
      !read.IsOk()) {
    return read;
  }
  return CheckEveryOptionListed(root, where);
}

StatusOr<lpc::Frame> ReadFrame(const YAML::Node& node, const Where& where) {
  lpc::Frame frame;
  bool has_energy = false;
  constexpr std::int64_t kFieldMax = 63;
  if (Status status = ForEachEntry(
          node, where,
          [&](const std::string& key, const YAML::Node& value) -> Status {
            const Where at = where.Key(key);
            if (key == "k") {
              if (!value.IsSequence() || value.size() > frame.k.size()) {
                return at.Error(
                    fmt::format("must be a list of at most {} "
                                "numbers",
                                frame.k.size()));
              }
              for (std::size_t i = 0; i < value.size(); ++i) {
                StatusOr<int> index =
                    ReadInteger(value[i], at.Index(i), 0, kFieldMax);
                if (!index.IsOk()) {
                  return index.GetStatus();
                }
                frame.k.at(i) = *index;
              }
              return OkStatus();
            }
            if (key != "energy" && key != "repeat" && key != "pitch") {
              return at.Error("is not a frame field");
            }
            StatusOr<int> number = ReadInteger(value, at, 0, kFieldMax);
            if (!number.IsOk()) {
              return number.GetStatus();
            }
            if (key == "energy") {
              frame.energy = *number;
              has_energy = true;
            } else if (key == "repeat") {
              if (*number > 1) {
                return at.Error("must be 0 or 1");
              }
              frame.repeat = *number == 1;
            } else {
              frame.pitch = *number;
            }
            return OkStatus();
          });
      !status.IsOk()) {
    return status;
  }
  if (!has_energy) {
    return where.Error("has no energy");
  }
  if (Status status = lpc::ValidateFrame(frame); !status.IsOk()) {
    return where.Error(status.Message());
  }
  return frame;
}

// Reads a quality mapping into `encoding`. Figures from other measures
// than quality::kMeasuresVersion's, or incomplete, are dropped, to be
// measured again (plan §4), as are the pesq and anchor keys the
// application wrote before its measures; other keys are kept.
Status ReadQuality(const YAML::Node& node, const Where& where,
                   CachedEncoding& encoding) {
  CachedQuality q;
  std::optional<int> version;
  std::set<std::string_view> found;
  if (const Status status = ForEachEntry(
          node, where,
          [&](const std::string& key, const YAML::Node& value) -> Status {
            const Where at = where.Key(key);
            if (key == "pesq" || key == "anchor") {
              return OkStatus();
            }
            if (key == "measures_version") {
              StatusOr<int> number = ReadInteger(value, at, 0, kIntMax);
              if (!number.IsOk()) {
                return number.GetStatus();
              }
              version = *number;
              return OkStatus();
            }
            const auto figure = std::ranges::find_if(
                kQualityFigures,
                [&key](const auto& entry) { return entry.first == key; });
            if (key != "estoi" && figure == kQualityFigures.end()) {
              encoding.quality_unknown.emplace_back(key, NodeText(value));
              return OkStatus();
            }
            StatusOr<double> number = ReadNumber(value, at);
            if (!number.IsOk()) {
              return number.GetStatus();
            }
            if (key == "estoi") {
              q.estoi = *number;
            } else {
              q.*(figure->second) = *number;
              found.insert(figure->first);
            }
            return OkStatus();
          });
      !status.IsOk()) {
    return status;
  }
  if (version == quality::kMeasuresVersion &&
      found.size() == kQualityFigures.size()) {
    q.measures_version = *version;
    encoding.quality = q;
  }
  return OkStatus();
}

StatusOr<CachedEncoding> ReadFramesYaml(const std::vector<std::uint8_t>& bytes,
                                        const std::string& file) {
  StatusOr<YAML::Node> root = ParseYaml(bytes, file);
  if (!root.IsOk()) {
    return root.GetStatus();
  }
  const Where where(file, "");
  CachedEncoding encoding;
  bool has_frames = false;
  if (Status status = ForEachEntry(
          *root, where,
          [&](const std::string& key, const YAML::Node& value) -> Status {
            const Where at = where.Key(key);
            if (key == "encoder_version" || key == "settings_hash") {
              StatusOr<std::string> text = ReadString(value, at);
              if (!text.IsOk()) {
                return text.GetStatus();
              }
              (key == "encoder_version" ? encoding.encoder_version
                                        : encoding.settings_hash) =
                  *std::move(text);
            } else if (key == "frames") {
              if (!value.IsSequence()) {
                return at.Error("must be a list");
              }
              for (std::size_t i = 0; i < value.size(); ++i) {
                StatusOr<lpc::Frame> frame = ReadFrame(value[i], at.Index(i));
                if (!frame.IsOk()) {
                  return frame.GetStatus();
                }
                encoding.frames.push_back(*frame);
              }
              has_frames = true;
            } else if (key == "quality") {
              return ReadQuality(value, at, encoding);
            } else {
              encoding.unknown.emplace_back(key, NodeText(value));
            }
            return OkStatus();
          });
      !status.IsOk()) {
    return status;
  }
  if (!has_frames) {
    return where.Error("has no frames");
  }
  return encoding;
}

}  // namespace

ProjectSerializer::ProjectSerializer(audio::IWavCodec& codec)
    : codec_(&codec) {}

StatusOr<ProjectFiles> ProjectSerializer::ToFiles(const Project& project) {
  const ProjectContents& contents = project.Contents();
  ProjectFiles files;
  auto add = [&files](std::string path,
                      std::vector<std::uint8_t> data) -> Status {
    if (!files.emplace(path, std::move(data)).second) {
      return InternalError(fmt::format("two files would be saved as {}", path));
    }
    return OkStatus();
  };
  try {
    if (Status status =
            add(std::string(kProjectYamlPath), Bytes(ProjectYaml(contents)));
        !status.IsOk()) {
      return status;
    }
    for (const Phrase& phrase : contents.phrases) {
      const std::string directory = PhraseDirectory(phrase.id);
      if (!phrase.audio.empty()) {
        StatusOr<std::vector<std::uint8_t>> wav = codec_->Encode(
            phrase.audio, kProjectSampleRate, audio::WavOutputFormat::kPcm24);
        if (!wav.IsOk()) {
          return wav.GetStatus();
        }
        if (Status status =
                add(directory + std::string(kSourceWavName), *std::move(wav));
            !status.IsOk()) {
          return status;
        }
      }
      if (phrase.encoding.has_value()) {
        if (Status status = add(directory + std::string(kFramesYamlName),
                                Bytes(FramesYaml(*phrase.encoding)));
            !status.IsOk()) {
          return status;
        }
      }
      for (const auto& [name, data] : phrase.extra_files) {
        if (Status status = add(directory + name, data); !status.IsOk()) {
          return status;
        }
      }
    }
    for (const auto& [path, data] : contents.extra_files) {
      if (Status status = add(path, data); !status.IsOk()) {
        return status;
      }
    }
  } catch (const YAML::Exception& error) {
    // Only an unknown key's stored text can fail to parse here.
    return InternalError(
        fmt::format("cannot write the project's YAML: {}", error.what()));
  }
  return files;
}

StatusOr<Project> ProjectSerializer::FromFiles(const ProjectFiles& files) {
  const auto project_yaml = files.find(std::string(kProjectYamlPath));
  if (project_yaml == files.end()) {
    return NotFoundError(fmt::format("not a Speak5220 project: there is no {}",
                                     kProjectYamlPath));
  }
  ProjectContents contents;
  try {
    StatusOr<YAML::Node> root =
        ParseYaml(project_yaml->second, kProjectYamlPath);
    if (!root.IsOk()) {
      return root.GetStatus();
    }
    if (Status status = ReadProjectYaml(*root, contents); !status.IsOk()) {
      return status;
    }
    std::set<std::string> used = {std::string(kProjectYamlPath)};
    for (Phrase& phrase : contents.phrases) {
      const std::string directory = PhraseDirectory(phrase.id);
      const std::string wav_path = directory + std::string(kSourceWavName);
      const std::string frames_path = directory + std::string(kFramesYamlName);
      if (const auto wav = files.find(wav_path); wav != files.end()) {
        StatusOr<audio::DecodedWav> decoded = codec_->Decode(wav->second);
        if (!decoded.IsOk()) {
          return DataLossError(
              fmt::format("{}: {}", wav_path, decoded.GetStatus().Message()));
        }
        if (decoded->sample_rate != kProjectSampleRate ||
            decoded->channels != 1) {
          return DataLossError(fmt::format(
              "{} must be {} Hz mono; it is {} Hz with {} channel(s)", wav_path,
              kProjectSampleRate, decoded->sample_rate, decoded->channels));
        }
        phrase.audio = std::move(decoded->samples);
        audio::QuantiseToPcm24(phrase.audio);
        used.insert(wav_path);
      } else if (phrase.source.has_value()) {
        return DataLossError(fmt::format(
            "{} is missing: phrase {} has no audio", wav_path, phrase.name));
      }
      if (const auto frames = files.find(frames_path); frames != files.end()) {
        StatusOr<CachedEncoding> encoding =
            ReadFramesYaml(frames->second, frames_path);
        if (!encoding.IsOk()) {
          return encoding.GetStatus();
        }
        phrase.encoding = *std::move(encoding);
        used.insert(frames_path);
      }
      // Other files of the phrase's directory go with the phrase.
      for (auto it = files.lower_bound(directory);
           it != files.end() && it->first.starts_with(directory); ++it) {
        if (!used.contains(it->first)) {
          phrase.extra_files.emplace(it->first.substr(directory.size()),
                                     it->second);
          used.insert(it->first);
        }
      }
    }
    for (const auto& [path, data] : files) {
      if (!used.contains(path)) {
        contents.extra_files.emplace(path, data);
      }
    }
  } catch (const YAML::Exception& error) {
    return DataLossError(
        fmt::format("the project's YAML cannot be read: {}", error.what()));
  }
  StatusOr<Project> project = Project::FromContents(std::move(contents));
  if (project.IsOk()) {
    Logger(kLogModuleProject)
        ->debug("read project \"{}\": {} phrase(s)", project->Settings().name,
                project->Phrases().size());
  }
  return project;
}

Status ProjectSerializer::Save(const Project& project, IProjectStore& store) {
  StatusOr<ProjectFiles> files = ToFiles(project);
  if (!files.IsOk()) {
    return files.GetStatus();
  }
  return store.Write(*files);
}

StatusOr<Project> ProjectSerializer::Load(IProjectStore& store) {
  StatusOr<ProjectFiles> files = store.Read();
  if (!files.IsOk()) {
    return files.GetStatus();
  }
  return FromFiles(*files);
}

}  // namespace speak5220::core::project
