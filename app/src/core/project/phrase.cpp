/*
 * File:        phrase.cpp
 * Module:      project
 * Purpose:     A phrase of a project: its name, source audio, encoder
 *              settings and cached encoding
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrase.h"

#include <QUuid>
#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <utility>

#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"

namespace speak5220::core::project {
namespace {

// The lengths of the five groups of a canonical UUID.
constexpr std::array<std::size_t, 5> kUuidGroups = {8, 4, 4, 4, 12};

bool IsLowerHex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

}  // namespace

bool IsValidPhraseId(std::string_view id) {
  std::size_t position = 0;
  for (std::size_t group = 0; group < kUuidGroups.size(); ++group) {
    if (group > 0) {
      if (position >= id.size() || id[position] != '-') {
        return false;
      }
      ++position;
    }
    for (std::size_t i = 0; i < kUuidGroups.at(group); ++i, ++position) {
      if (position >= id.size() || !IsLowerHex(id[position])) {
        return false;
      }
    }
  }
  return position == id.size();
}

PhraseId NewPhraseId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces).toStdString();
}

StatusOr<encoder::EncoderOptions> EffectiveEncoderOptions(
    const encoder::EncoderOptions& defaults,
    const EncoderOverrides& overrides) {
  encoder::EncoderOptions options = defaults;
  for (const auto& [field, text] : overrides) {
    if (Status status = encoder::SetEncoderOption(options, field, text);
        !status.IsOk()) {
      return status;
    }
  }
  return options;
}

EncoderOverrides OverridesFor(const encoder::EncoderOptions& defaults,
                              const encoder::EncoderOptions& options) {
  EncoderOverrides overrides;
  for (const encoder::EncoderOptionInfo& info : encoder::EncoderOptionInfos()) {
    std::string text = encoder::EncoderOptionText(options, info.field);
    if (text != encoder::EncoderOptionText(defaults, info.field)) {
      overrides.emplace(std::string(info.field), std::move(text));
    }
  }
  return overrides;
}

StatusOr<int> PackedSize(std::span<const lpc::Frame> frames) {
  // The length FrameCodec::Pack() gives for lpc::EnsureStop(frames),
  // counted from the frame lengths rather than packed, since the project
  // asks for every phrase's size on each edit.
  constexpr int kBitsPerByte = 8;
  int bits = 0;
  bool stopped = false;
  for (const lpc::Frame& frame : frames) {
    if (Status status = lpc::ValidateFrame(frame); !status.IsOk()) {
      return status;
    }
    bits += lpc::FrameBits(frame);
    if (lpc::FrameKind(frame) == lpc::FrameType::kStop) {
      stopped = true;
      break;
    }
  }
  if (!stopped) {
    bits += lpc::FrameBits(lpc::Frame::Stop());
  }
  return (bits + kBitsPerByte - 1) / kBitsPerByte;
}

}  // namespace speak5220::core::project
