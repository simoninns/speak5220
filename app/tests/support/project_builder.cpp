/*
 * File:        project_builder.cpp
 * Module:      test support
 * Purpose:     Builds phrases and projects for tests: fixed ids, frames
 *              that pack to a chosen size, phrases with or without audio
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "support/project_builder.h"

#include <spdlog/fmt/fmt.h>

#include <string>
#include <utility>
#include <vector>

#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project.h"
#include "gtest/gtest.h"

namespace speak5220::testing {

namespace project = core::project;

project::PhraseId TestId(int n) {
  return fmt::format("00000000-0000-0000-0000-{:012x}", n);
}

std::vector<core::lpc::Frame> FramesPackingTo(int bytes) {
  // 8 * bytes bits exactly: an even number of 50-bit voiced frames (so the
  // rest stays a multiple of 4), then 4-bit silence frames and the stop.
  constexpr int kVoicedBits = 50;
  constexpr int kShortBits = 4;
  const int bits = 8 * bytes - kShortBits;
  int voiced = bits / kVoicedBits;
  voiced -= voiced % 2;
  const int silent = (bits - voiced * kVoicedBits) / kShortBits;
  std::vector<core::lpc::Frame> frames(
      static_cast<std::size_t>(voiced),
      core::lpc::Frame::Voiced(5, 30, {10, 10, 8, 8, 8, 8, 8, 4, 4, 4}));
  frames.insert(frames.end(), static_cast<std::size_t>(silent),
                core::lpc::Frame::Silence());
  frames.push_back(core::lpc::Frame::Stop());
  return frames;
}

int BytesForCost(int cost, const std::string& name) {
  return cost - static_cast<int>(name.size()) - project::kPhromPointerBytes;
}

project::Phrase AudioPhrase(int n, const std::string& name) {
  project::Phrase phrase;
  phrase.id = TestId(n);
  phrase.name = name;
  phrase.audio = {0.0F, 0.25F, -0.25F, 0.5F};
  phrase.source = project::SourceInfo{.file = "source.wav",
                                      .rate = project::kProjectSampleRate,
                                      .channels = 1,
                                      .duration = 0.0005,
                                      .sha256 = std::string(64, '0'),
                                      .unknown = {}};
  return phrase;
}

project::Phrase EncodedPhrase(int n, const std::string& name, int cost) {
  project::Phrase phrase = AudioPhrase(n, name);
  phrase.encoding = project::CachedEncoding{
      .encoder_version = "test",
      .settings_hash = "",
      .frames = FramesPackingTo(BytesForCost(cost, name)),
      .pesq = std::nullopt,
      .anchor = std::nullopt,
      .unknown = {},
      .quality_unknown = {}};
  return phrase;
}

project::Phrase LockedPhrase(int n, const std::string& name, int cost) {
  project::Phrase phrase = EncodedPhrase(n, name, cost);
  phrase.audio.clear();
  phrase.source.reset();
  phrase.locked = true;
  return phrase;
}

project::Project ProjectWith(std::vector<project::Phrase> phrases) {
  project::ProjectContents contents;
  contents.phrases = std::move(phrases);
  core::StatusOr<project::Project> made =
      project::Project::FromContents(std::move(contents));
  EXPECT_TRUE(made.IsOk()) << made.GetStatus().ToString();
  return made.IsOk() ? *std::move(made) : project::Project();
}

}  // namespace speak5220::testing
