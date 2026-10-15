/*
 * File:        docs_coverage_test.cpp
 * Module:      docs tests
 * Purpose:     Checks that the reference pages of the user documentation
 *              cover every encoder option, CLI option and command,
 *              project-file key and export format the code has
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <yaml-cpp/yaml.h>

#include <QProcess>
#include <QString>
#include <QStringList>
#include <algorithm>
#include <array>
#include <cstddef>
#include <filesystem>
#include <format>
#include <fstream>
#include <iterator>
#include <set>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "core/encoder/encoder_options.h"
#include "core/project/directory_project_store.h"
#include "core/project/phrase.h"
#include "core/project/project.h"
#include "core/project/project_export.h"
#include "core/project/project_serializer.h"
#include "core/project/project_store.h"
#include "core/quality/quality_assessor.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220 {
namespace {

namespace fs = std::filesystem;
namespace encoder = core::encoder;
namespace project = core::project;

// Time allowed for one run of the CLI.
constexpr int kProcessTimeoutMs = 30000;

// The fixture whose phrase, encoding and preset the project is built from.
constexpr std::string_view kFixture = "general/one-phrase";

// The most words an inline code span that names items has, as in
// `export --format acorn-phrom`.
constexpr std::size_t kMaxMentionWords = 4;

// A reference page, read once: its lines outside fenced code blocks, and
// the words of its inline code spans.
class ReferencePage {
 public:
  explicit ReferencePage(std::string name) : name_(std::move(name)) {
    std::ifstream file(fs::path(SPEAK5220_DOCS_DIR) / "reference" / name_);
    EXPECT_TRUE(file.is_open()) << "cannot read " << name_;
    bool fenced = false;
    for (std::string line; std::getline(file, line);) {
      const std::size_t first = line.find_first_not_of(' ');
      if (first != std::string::npos && line.compare(first, 3, "```") == 0) {
        fenced = !fenced;
        continue;
      }
      if (!fenced) {
        AddCodeWords(line);
        lines_.push_back(std::move(line));
      }
    }
  }

  const std::string& Name() const { return name_; }

  // True if `word` is one of the words of an inline code span of at most
  // kMaxMentionWords words: the span split at spaces and commas, with
  // quotes and brackets stripped, so `"none"`, `--help`, `-h` and
  // `export --format wav` all count.
  bool Mentions(std::string_view word) const {
    return code_words_.contains(std::string(word));
  }

  // The cells of the table row whose first cell starts with `word` in
  // code (| `word` ... |), or nothing.
  std::vector<std::string> RowFor(std::string_view word) const {
    const std::string start = std::format("| `{}`", word);
    for (const std::string& line : lines_) {
      if (line.starts_with(start) &&
          (line.size() == start.size() || line[start.size()] != '`')) {
        return Cells(line);
      }
    }
    return {};
  }

  // The cells of the header row of the table whose row RowFor() finds.
  std::vector<std::string> HeaderFor(std::string_view word) const {
    const std::string start = std::format("| `{}`", word);
    std::vector<std::string> header;
    for (std::size_t i = 0; i < lines_.size(); ++i) {
      if (i + 1 < lines_.size() && lines_[i].starts_with('|') &&
          lines_[i + 1].starts_with("|---")) {
        header = Cells(lines_[i]);
      }
      if (lines_[i].starts_with(start)) {
        return header;
      }
    }
    return {};
  }

 private:
  static std::vector<std::string> Cells(std::string line) {
    // An escaped bar is part of a cell.
    for (std::size_t at = line.find("\\|"); at != std::string::npos;
         at = line.find("\\|", at)) {
      line.replace(at, 2, "/");
    }
    std::vector<std::string> cells;
    std::stringstream stream(line);
    std::string cell;
    std::getline(stream, cell, '|');  // Before the first bar.
    while (std::getline(stream, cell, '|')) {
      const std::size_t first = cell.find_first_not_of(' ');
      const std::size_t last = cell.find_last_not_of(' ');
      cells.push_back(first == std::string::npos
                          ? std::string()
                          : cell.substr(first, last - first + 1));
    }
    return cells;
  }

  void AddCodeWords(const std::string& line) {
    std::size_t open = line.find('`');
    while (open != std::string::npos) {
      const std::size_t close = line.find('`', open + 1);
      if (close == std::string::npos) {
        break;
      }
      std::string span = line.substr(open + 1, close - open - 1);
      std::ranges::replace(span, ',', ' ');
      std::stringstream stream(span);
      std::vector<std::string> words;
      for (std::string word; stream >> word;) {
        constexpr std::string_view kStrip = "\"'()[]{}";
        const std::size_t first = word.find_first_not_of(kStrip);
        const std::size_t last = word.find_last_not_of(kStrip);
        if (first != std::string::npos) {
          words.push_back(word.substr(first, last - first + 1));
        }
      }
      // A longer span is a sentence, such as a message, not a mention.
      if (words.size() <= kMaxMentionWords) {
        code_words_.insert(words.begin(), words.end());
      }
      open = line.find('`', close + 1);
    }
  }

  std::string name_;
  std::vector<std::string> lines_;
  std::set<std::string> code_words_;
};

// Joins `items` for a failure message, one per line.
std::string List(const std::set<std::string>& items) {
  std::string text;
  for (const std::string& item : items) {
    text += "\n  " + item;
  }
  return text;
}

std::string RunCli(const QStringList& arguments) {
  QProcess process;
  process.start(QStringLiteral(SPEAK5220_CLI_PATH), arguments);
  EXPECT_TRUE(process.waitForFinished(kProcessTimeoutMs))
      << "speak5220-cli did not finish";
  return (QString::fromUtf8(process.readAllStandardOutput()) +
          QString::fromUtf8(process.readAllStandardError()))
      .toStdString();
}

// The long options `text` names: "--", a lower-case letter, then lower-case
// letters and hyphens. Scanned by hand: std::regex trips GCC's
// -Wmaybe-uninitialized in Release and sanitizer builds.
std::set<std::string> LongOptions(std::string_view text) {
  const auto is_lower = [](char c) { return c >= 'a' && c <= 'z'; };
  std::set<std::string> options;
  std::size_t at = text.find("--");
  while (at != std::string_view::npos) {
    std::size_t end = at + 2;
    if (end < text.size() && is_lower(text[end])) {
      while (end < text.size() && (is_lower(text[end]) || text[end] == '-')) {
        ++end;
      }
      options.emplace(text.substr(at, end - at));
      at = text.find("--", end);
    } else {
      at = text.find("--", at + 1);
    }
  }
  return options;
}

// The column of `header` called `name`, or std::string::npos.
std::size_t Column(const std::vector<std::string>& header,
                   std::string_view name) {
  const auto found = std::ranges::find(header, name);
  return found == header.end()
             ? std::string::npos
             : static_cast<std::size_t>(std::distance(header.begin(), found));
}

TEST(DocsCoverageTest, EveryEncoderOptionHasARowWithItsTypeDefaultAndRange) {
  const ReferencePage page("encoder-options.md");
  std::set<std::string> missing;
  std::set<std::string> wrong;
  for (const encoder::EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    const std::vector<std::string> row = page.RowFor(info.field);
    const std::vector<std::string> header = page.HeaderFor(info.field);
    if (row.empty()) {
      missing.insert(std::string(info.field));
      continue;
    }
    const std::size_t type_column = Column(header, "Type");
    const std::size_t default_column = Column(header, "Default");
    const std::size_t range_column = Column(header, "Range");
    const std::size_t slider_column = Column(header, "Slider");
    if (std::max({type_column, default_column, range_column, slider_column}) >=
        row.size()) {
      wrong.insert(std::format("{}: no Type, Default, Range and Slider columns",
                               info.field));
      continue;
    }
    std::string type;
    switch (info.type) {
      case encoder::EncoderOptionType::kReal:
        type = "Number";
        break;
      case encoder::EncoderOptionType::kInteger:
        type = "Whole number";
        break;
      case encoder::EncoderOptionType::kOptionalReal:
        type = "Number, or `null`";
        break;
    }
    if (!row[type_column].starts_with(type)) {
      wrong.insert(std::format(R"({}: type "{}", expected "{}")", info.field,
                               row[type_column], type));
    }
    // An optional option whose default is unset has no default text.
    const std::string default_text =
        info.default_text.empty() ||
                info.default_text == encoder::kUnsetOptionText
            ? std::string("Unset")
            : std::string(info.default_text);
    const std::string& documented = row[default_column];
    if (!documented.starts_with(default_text) ||
        (documented.size() > default_text.size() &&
         documented[default_text.size()] != ' ')) {
      wrong.insert(std::format(R"({}: default "{}", expected "{}")", info.field,
                               documented, default_text));
    }
    const std::string range =
        std::format("{} to {}", info.minimum, info.maximum);
    if (!row[range_column].ends_with(range)) {
      wrong.insert(std::format(R"({}: range "{}", expected "{}")", info.field,
                               row[range_column], range));
    }
    // The slider's span and step, as "12 to 60 dB, steps of 1" or
    // "1 (off) to 6, steps of 0.05".
    const std::string& slider = row[slider_column];
    const std::string typical_minimum =
        std::format("{} ", info.typical_minimum);
    const std::string typical_maximum =
        std::format("to {}", info.typical_maximum);
    const std::string step = std::format("steps of {}", info.step);
    if (slider.find(typical_minimum) == std::string::npos ||
        slider.find(typical_maximum) == std::string::npos ||
        !slider.ends_with(step)) {
      wrong.insert(std::format(R"({}: slider "{}", expected {}{}, {})",
                               info.field, slider, typical_minimum,
                               typical_maximum, step));
    }
  }
  EXPECT_TRUE(missing.empty())
      << page.Name() << " has no row for the options:" << List(missing);
  EXPECT_TRUE(wrong.empty())
      << page.Name() << " disagrees with the code:" << List(wrong);
}

TEST(DocsCoverageTest, EveryCliOptionHasARow) {
  const std::string help = RunCli({QStringLiteral("--help")});
  const std::set<std::string> options = LongOptions(help);
  // The options of main() and of AddCommandOptions().
  ASSERT_TRUE(options.contains("--log-level")) << help;
  ASSERT_TRUE(options.contains("--output")) << help;

  const ReferencePage page("cli-options.md");
  std::set<std::string> missing;
  for (const std::string& option : options) {
    if (page.RowFor(option).empty()) {
      missing.insert(option);
    }
  }
  EXPECT_TRUE(missing.empty())
      << page.Name() << " has no row for the options:" << List(missing);
}

TEST(DocsCoverageTest, EveryCliCommandHasARow) {
  // An unknown command's message lists every command RunCommand() runs:
  // "use new, add, ... or measure".
  const std::string message = RunCli({QStringLiteral("no-such-command")});
  const std::size_t use = message.find("use ");
  ASSERT_NE(use, std::string::npos) << message;
  std::string list = message.substr(use + 4);
  list = list.substr(0, list.find_first_of("\r\n"));
  std::ranges::replace(list, ',', ' ');
  std::set<std::string> commands;
  std::stringstream words(list);
  for (std::string word; words >> word;) {
    if (word != "or") {
      commands.insert(word);
    }
  }
  ASSERT_TRUE(commands.contains("new")) << message;

  const ReferencePage page("cli-options.md");
  std::set<std::string> missing;
  for (const std::string& command : commands) {
    if (page.RowFor(command).empty()) {
      missing.insert(command);
    }
  }
  EXPECT_TRUE(missing.empty())
      << page.Name() << " has no row for the commands:" << List(missing);
}

// A project from kFixture with every optional part filled: an ASCII map,
// PHROM headers, a described phrase that sets every option and has a
// quality with ESTOI, a locked phrase with no source, and a preset of
// each format with every option its format takes.
project::Project ProjectWithEverything(core::audio::IWavCodec& codec) {
  project::DirectoryProjectStore store(fs::path(SPEAK5220_PROJECTS_DIR) /
                                       kFixture);
  project::ProjectSerializer serializer(codec);
  core::StatusOr<project::Project> loaded = serializer.Load(store);
  EXPECT_TRUE(loaded.IsOk()) << loaded.GetStatus().ToString();
  if (!loaded.IsOk()) {
    return {};
  }
  project::ProjectContents contents = loaded->Contents();
  EXPECT_FALSE(contents.phrases.empty());
  if (contents.phrases.empty()) {
    return {};
  }
  project::Phrase& phrase = contents.phrases.front();
  EXPECT_TRUE(phrase.source.has_value() && phrase.encoding.has_value());
  if (!phrase.encoding.has_value()) {
    return {};
  }
  phrase.description = "Described";
  for (const encoder::EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    phrase.encoder[std::string(info.field)] = encoder::EncoderOptionText(
        contents.settings.default_encoder, info.field);
  }
  phrase.encoding->quality = project::CachedQuality{
      .measures_version = core::quality::kMeasuresVersion,
      .lsd_db = 3.0,
      .lsd_over_2db = 50.0,
      .lsd_over_4db = 20.0,
      .gross_pitch_error = 5.0,
      .f0_rmse_cents = 60.0,
      .voicing_error = 10.0,
      .estoi = 0.75,
      .score = 0.7};

  const std::string name = phrase.name;
  project::Phrase locked = phrase;
  locked.id = "00000000-0000-0000-0000-000000000001";
  locked.name = "LOCKED";
  locked.locked = true;
  locked.source.reset();
  locked.audio.clear();
  locked.encoder.clear();
  // The push may reallocate, so `phrase` is not used after it.
  contents.phrases.push_back(std::move(locked));

  contents.settings.ascii_map[' '] = name;
  contents.settings.phroms.push_back(
      {.number = 1, .header_strings = {"(C)2026 Docs", "First", "1.00"}});

  contents.exports.clear();
  for (const project::ExportFormatInfo& format : project::ExportFormats()) {
    project::ExportPreset preset{.name = std::string(format.key),
                                 .format = std::string(format.key),
                                 .scope = std::string(project::kScopeSelected),
                                 .phrases = {name}};
    if (format.takes_layout) {
      preset.layout = std::string(project::kLayoutConcatenated);
    }
    if (format.takes_licence) {
      preset.licence = "CC0-1.0";
    }
    if (format.takes_audio) {
      preset.audio = std::string(project::kAudioOriginal);
    }
    contents.exports.push_back(std::move(preset));
  }

  core::StatusOr<project::Project> filled =
      project::Project::FromContents(std::move(contents));
  EXPECT_TRUE(filled.IsOk()) << filled.GetStatus().ToString();
  return filled.IsOk() ? *std::move(filled) : project::Project();
}

// The mappings whose keys are option fields (EncoderOptionInfo::field),
// documented in encoder-options.md, and the one whose keys are data.
constexpr std::array<std::string_view, 4> kOptionMappings = {
    "default_preprocess", "default_encoder", "preprocess", "encoder"};
constexpr std::string_view kAsciiMap = "ascii_map";

// Adds the keys of every mapping in `node` to `keys`, and those of the
// option mappings to `option_keys`.
void CollectKeys(const YAML::Node& node, std::set<std::string>& keys,
                 std::set<std::string>& option_keys) {
  if (node.IsSequence()) {
    for (const YAML::Node& item : node) {
      CollectKeys(item, keys, option_keys);
    }
    return;
  }
  if (!node.IsMap()) {
    return;
  }
  for (const auto& entry : node) {
    const std::string key = entry.first.Scalar();
    if (std::ranges::find(kOptionMappings, key) != kOptionMappings.end()) {
      keys.insert(key);
      for (const auto& option : entry.second) {
        option_keys.insert(option.first.Scalar());
      }
      continue;
    }
    keys.insert(key);
    if (key != kAsciiMap) {
      CollectKeys(entry.second, keys, option_keys);
    }
  }
}

TEST(DocsCoverageTest, EveryProjectFileKeyIsDocumented) {
  core::audio::SndfileWavCodec codec;
  const project::Project everything = ProjectWithEverything(codec);
  ASSERT_EQ(everything.Phrases().size(), 2U);
  project::ProjectSerializer serializer(codec);
  const core::StatusOr<project::ProjectFiles> files =
      serializer.ToFiles(everything);
  ASSERT_TRUE(files.IsOk()) << files.GetStatus().ToString();

  std::set<std::string> keys;
  std::set<std::string> option_keys;
  int frames_files = 0;
  for (const auto& [path, bytes] : *files) {
    if (path != project::kProjectYamlPath &&
        !path.ends_with(project::kFramesYamlName)) {
      continue;
    }
    frames_files += path.ends_with(project::kFramesYamlName) ? 1 : 0;
    const YAML::Node root = YAML::Load(std::string(bytes.begin(), bytes.end()));
    CollectKeys(root, keys, option_keys);
  }
  ASSERT_EQ(frames_files, 2);
  // The parts ProjectWithEverything() fills.
  for (const char* key :
       {"ascii_map", "phroms", "number", "source", "preprocess", "encoder",
        "estoi", "layout", "licence", "audio", "phrases"}) {
    EXPECT_TRUE(keys.contains(key)) << "the project has no " << key;
  }

  const ReferencePage project_file("project-file.md");
  std::set<std::string> missing;
  for (const std::string& key : keys) {
    if (!project_file.Mentions(key)) {
      missing.insert(key);
    }
  }
  EXPECT_TRUE(missing.empty())
      << project_file.Name() << " does not mention the keys:" << List(missing);

  const ReferencePage options("encoder-options.md");
  std::set<std::string> missing_options;
  for (const std::string& key : option_keys) {
    if (encoder::FindEncoderOption(key) == nullptr ||
        options.RowFor(key).empty()) {
      missing_options.insert(key);
    }
  }
  EXPECT_TRUE(missing_options.empty())
      << options.Name()
      << " has no row for the option keys:" << List(missing_options);
}

TEST(DocsCoverageTest, EveryExportFormatAndPresetValueIsDocumented) {
  const ReferencePage formats("export-formats.md");
  const ReferencePage project_file("project-file.md");
  std::set<std::string> missing;
  for (const project::ExportFormatInfo& format : project::ExportFormats()) {
    if (formats.RowFor(format.key).empty()) {
      missing.insert(
          std::format("format {} (a row of {})", format.key, formats.Name()));
    }
  }
  for (const std::string_view value :
       {project::kScopeAll, project::kScopeSelected, project::kLayoutSeparate,
        project::kLayoutConcatenated, project::kAudioEncoded,
        project::kAudioOriginal, project::kAudioPreprocessed,
        project::kDefaultCHeaderLicence}) {
    if (!formats.Mentions(value) && !project_file.Mentions(value)) {
      missing.insert(std::string(value));
    }
  }
  EXPECT_TRUE(missing.empty())
      << "not documented in the reference pages:" << List(missing);
}

}  // namespace
}  // namespace speak5220
