/*
 * File:        golden_data.cpp
 * Module:      test support
 * Purpose:     Reads the reference data in app/golden/: codes, event logs,
 *              tables, images and frame lists
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/golden_data.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace speak5220::testing {
namespace {

using core::DataLossError;
using core::NotFoundError;
using core::StatusOr;
using core::lpc::Frame;

std::vector<std::string> Split(const std::string& line, char separator) {
  std::vector<std::string> fields;
  std::string field;
  std::istringstream stream(line);
  while (std::getline(stream, field, separator)) {
    fields.push_back(field);
  }
  if (!line.empty() && line.back() == separator) {
    fields.emplace_back();
  }
  return fields;
}

std::optional<int> ParseInt(const std::string& text) {
  std::istringstream stream(text);
  int value = 0;
  if (!(stream >> value) || !stream.eof()) {
    return std::nullopt;
  }
  return value;
}

}  // namespace

StatusOr<std::vector<std::string>> ReadLines(
    const std::filesystem::path& path) {
  std::ifstream file(path);
  if (!file) {
    return NotFoundError("missing reference file " + path.string());
  }
  std::vector<std::string> lines;
  std::string line;
  while (std::getline(file, line)) {
    lines.push_back(line);
  }
  return lines;
}

StatusOr<std::vector<std::uint8_t>> ReadBytes(
    const std::filesystem::path& path) {
  std::ifstream file(path, std::ios::binary);
  if (!file) {
    return NotFoundError("missing reference file " + path.string());
  }
  return std::vector<std::uint8_t>(std::istreambuf_iterator<char>(file),
                                   std::istreambuf_iterator<char>());
}

StatusOr<std::vector<int>> ReadCodes(const std::filesystem::path& path) {
  StatusOr<std::vector<std::string>> lines = ReadLines(path);
  if (!lines.IsOk()) {
    return lines.GetStatus();
  }
  std::vector<int> codes;
  codes.reserve(lines->size());
  for (const std::string& line : *lines) {
    const std::optional<int> value = ParseInt(line);
    if (!value) {
      return DataLossError(path.string() + ": bad code \"" + line + "\"");
    }
    codes.push_back(*value);
  }
  return codes;
}

StatusOr<std::vector<std::map<std::string, std::string>>> ReadTable(
    const std::filesystem::path& path) {
  StatusOr<std::vector<std::string>> lines = ReadLines(path);
  if (!lines.IsOk()) {
    return lines.GetStatus();
  }
  if (lines->empty()) {
    return DataLossError(path.string() + " has no header row");
  }
  const std::vector<std::string> header = Split(lines->front(), '\t');
  std::vector<std::map<std::string, std::string>> rows;
  for (std::size_t n = 1; n < lines->size(); ++n) {
    const std::vector<std::string> fields = Split((*lines)[n], '\t');
    if (fields.size() != header.size()) {
      return DataLossError(path.string() + ": row " + std::to_string(n) +
                           " has the wrong number of columns");
    }
    std::map<std::string, std::string> row;
    for (std::size_t i = 0; i < header.size(); ++i) {
      row[header[i]] = fields[i];
    }
    rows.push_back(std::move(row));
  }
  return rows;
}

StatusOr<std::vector<Frame>> ReadFramesCsv(const std::filesystem::path& path) {
  StatusOr<std::vector<std::string>> lines = ReadLines(path);
  if (!lines.IsOk()) {
    return lines.GetStatus();
  }
  // frame, kind, energy, repeat, pitch, k1..k10
  constexpr std::size_t kColumns = 15;
  constexpr std::size_t kFirstK = 5;
  std::vector<Frame> frames;
  for (std::size_t n = 1; n < lines->size(); ++n) {
    const std::vector<std::string> fields = Split((*lines)[n], ',');
    if (fields.size() != kColumns) {
      return DataLossError(path.string() + ": bad row " + std::to_string(n));
    }
    std::array<std::optional<int>, kColumns> values = {};
    for (std::size_t i = 2; i < kColumns; ++i) {
      if (!fields[i].empty()) {
        values.at(i) = ParseInt(fields[i]);
        if (!values.at(i)) {
          return DataLossError(path.string() + ": bad value in row " +
                               std::to_string(n));
        }
      }
    }
    Frame frame;
    frame.energy = values[2].value_or(0);
    frame.repeat = values[3].value_or(0) != 0;
    frame.pitch = values[4].value_or(0);
    for (std::size_t i = 0; i < frame.k.size(); ++i) {
      frame.k.at(i) = values.at(kFirstK + i).value_or(0);
    }
    frames.push_back(frame);
  }
  return frames;
}

}  // namespace speak5220::testing
