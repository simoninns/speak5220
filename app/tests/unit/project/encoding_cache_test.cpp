/*
 * File:        encoding_cache_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the encoding cache with a mocked encoder: the
 *              hash, staleness, re-encoding and the project budget
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/encoding_cache.h"

#include <string>
#include <utility>
#include <vector>

#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/lpc/frame.h"
#include "core/project/phrase.h"
#include "core/project/phrom_allocator.h"
#include "core/project/project.h"
#include "core/util/status.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"

namespace speak5220::core::project {
namespace {

using ::testing::_;
using testing::AudioPhrase;
using ::testing::Field;
using testing::FramesPackingTo;
using testing::LockedPhrase;
using testing::MockEncoder;
using testing::ProjectWith;
using ::testing::Return;
using testing::TestId;

encoder::EncodeResult ResultPackingTo(int bytes) {
  return {.frames = FramesPackingTo(bytes), .details = std::nullopt};
}

// A phrase whose encoding is current for the default options.
Phrase CurrentPhrase(int n, const std::string& name) {
  Phrase phrase = AudioPhrase(n, name);
  phrase.encoding = CachedEncoding{
      .encoder_version = std::string(encoder::kEncoderVersion),
      .settings_hash = EncodingCache::SettingsHash(
          phrase.audio, encoder::EncoderOptions{}, encoder::kEncoderVersion),
      .frames = FramesPackingTo(10),
      .quality = std::nullopt,
      .unknown = {},
      .quality_unknown = {}};
  return phrase;
}

TEST(EncodingCacheTest, HashDependsOnAudioOptionsAndVersion) {
  const std::vector<float> audio = {0.0F, 0.5F, -0.25F};
  const encoder::EncoderOptions defaults;
  const std::string hash =
      EncodingCache::SettingsHash(audio, defaults, encoder::kEncoderVersion);
  EXPECT_EQ(hash.size(), 64U);
  EXPECT_EQ(hash, EncodingCache::SettingsHash(audio, defaults,
                                              encoder::kEncoderVersion));

  std::vector<float> other_audio = audio;
  other_audio[1] = 0.5F + (1.0F / 8388608.0F);
  EXPECT_NE(hash, EncodingCache::SettingsHash(other_audio, defaults,
                                              encoder::kEncoderVersion));
  encoder::EncoderOptions other_options;
  other_options.pre_emphasis = encoder::PreEmphasis::FirstOrder(0.9);
  EXPECT_NE(hash, EncodingCache::SettingsHash(audio, other_options,
                                              encoder::kEncoderVersion));
  EXPECT_NE(hash, EncodingCache::SettingsHash(audio, defaults, "0.0.1"));
  encoder::EncoderOptions squelched;
  squelched.preprocess.squelch_db = 30.0;
  const std::string squelched_hash =
      EncodingCache::SettingsHash(audio, squelched, encoder::kEncoderVersion);
  EXPECT_NE(hash, squelched_hash);
  squelched.preprocess.squelch_db = 24.0;
  EXPECT_NE(squelched_hash, EncodingCache::SettingsHash(
                                audio, squelched, encoder::kEncoderVersion));
}

// Every option counts, the preprocessing's as much as the encoder's.
TEST(EncodingCacheTest, PreprocessingAndEncoderOptionsBothCount) {
  const encoder::EncoderOptions defaults;
  const std::vector<float> silence;
  const std::string hash =
      EncodingCache::SettingsHash(silence, defaults, encoder::kEncoderVersion);
  encoder::EncoderOptions smoothed = defaults;
  smoothed.voicing_min_run = 3;
  encoder::EncoderOptions tilted = defaults;
  tilted.preprocess.treble_tilt = 0.4;
  encoder::EncoderOptions cut = defaults;
  cut.preprocess.bass_cut_hz = 200.0;
  for (const encoder::EncoderOptions& options : {smoothed, tilted, cut}) {
    EXPECT_NE(
        EncodingCache::SettingsHash(silence, options, encoder::kEncoderVersion),
        hash);
  }
}

TEST(EncodingCacheTest, PhraseWithoutAnEncodingIsNotCurrent) {
  EXPECT_FALSE(*EncodingCache::IsCurrent(AudioPhrase(1, "A"),
                                         encoder::EncoderOptions{}));
}

TEST(EncodingCacheTest, EncodingWithMatchingHashIsCurrent) {
  EXPECT_TRUE(*EncodingCache::IsCurrent(CurrentPhrase(1, "A"),
                                        encoder::EncoderOptions{}));
}

TEST(EncodingCacheTest, ChangedSettingsOrVersionMakeAnEncodingStale) {
  const Phrase phrase = CurrentPhrase(1, "A");
  encoder::EncoderOptions new_defaults;
  new_defaults.gate_db = 50.0;
  EXPECT_FALSE(*EncodingCache::IsCurrent(phrase, new_defaults));

  Phrase overridden = phrase;
  overridden.encoder["gate_db"] = "50";
  EXPECT_FALSE(
      *EncodingCache::IsCurrent(overridden, encoder::EncoderOptions{}));
  // The hash covers the options in effect, so an encoding stays current
  // when the defaults change under an override that sets the same value.
  overridden.encoding->settings_hash = EncodingCache::SettingsHash(
      overridden.audio, new_defaults, encoder::kEncoderVersion);
  EXPECT_TRUE(*EncodingCache::IsCurrent(overridden, encoder::EncoderOptions{}));
  EXPECT_TRUE(*EncodingCache::IsCurrent(overridden, new_defaults));

  Phrase old_version = phrase;
  old_version.encoding->encoder_version = "0.0.1";
  EXPECT_FALSE(
      *EncodingCache::IsCurrent(old_version, encoder::EncoderOptions{}));
}

TEST(EncodingCacheTest, LockedPhraseIsAlwaysCurrent) {
  EXPECT_TRUE(*EncodingCache::IsCurrent(LockedPhrase(1, "L", 10),
                                        encoder::EncoderOptions{}));
}

TEST(EncodingCacheTest, EncodesWithTheEffectiveOptionsAt8kHz) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  const Phrase phrase = AudioPhrase(1, "A");
  EXPECT_CALL(encoder,
              Encode(_, kProjectSampleRate,
                     Field(&encoder::EncoderOptions::target_rms, 18.0)))
      .WillOnce(Return(ResultPackingTo(5)));
  StatusOr<CachedEncoding> encoding =
      cache.Encode(phrase, encoder::EncoderOptions{}, {{"target_rms", "18"}});
  ASSERT_TRUE(encoding.IsOk());
  EXPECT_EQ(encoding->frames, FramesPackingTo(5));
  EXPECT_EQ(encoding->encoder_version, encoder::kEncoderVersion);
  encoder::EncoderOptions options;
  options.target_rms = 18.0;
  EXPECT_EQ(encoding->settings_hash,
            EncodingCache::SettingsHash(phrase.audio, options,
                                        encoder::kEncoderVersion));
}

TEST(EncodingCacheTest, EncoderFailureIsReportedWithThePhrasesName) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  EXPECT_CALL(encoder, Encode(_, _, _))
      .WillOnce(Return(InvalidArgumentError("non-finite sample")));
  const StatusOr<CachedEncoding> encoding =
      cache.Encode(AudioPhrase(1, "HELLO"), encoder::EncoderOptions{}, {});
  ASSERT_FALSE(encoding.IsOk());
  EXPECT_EQ(encoding.GetStatus().Code(), StatusCode::kInvalidArgument);
  EXPECT_NE(encoding.GetStatus().Message().find("HELLO"), std::string::npos);
}

TEST(EncodingCacheTest, PhraseWithoutAudioCannotBeEncoded) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  EXPECT_CALL(encoder, Encode(_, _, _)).Times(0);
  Phrase phrase = LockedPhrase(1, "L", 10);
  EXPECT_FALSE(cache.Encode(phrase, encoder::EncoderOptions{}, {}).IsOk());
}

TEST(EncodingCacheTest, SettingsChangeReencodes) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  Project project = ProjectWith({CurrentPhrase(1, "A")});
  EXPECT_CALL(encoder,
              Encode(_, _, Field(&encoder::EncoderOptions::window, 200)))
      .WillOnce(Return(ResultPackingTo(30)));
  StatusOr<EncodingChange> previous =
      cache.ChangeSettings(project, TestId(1), {{"window", "200"}});
  ASSERT_TRUE(previous.IsOk());
  EXPECT_TRUE(previous->overrides.empty());
  EXPECT_EQ(previous->encoding->frames, FramesPackingTo(10));
  const Phrase& phrase = *project.FindPhrase(TestId(1));
  EXPECT_EQ(phrase.encoder.at("window"), "200");
  EXPECT_EQ(phrase.encoding->frames, FramesPackingTo(30));
  EXPECT_TRUE(
      *EncodingCache::IsCurrent(phrase, project.Settings().default_encoder));
}

TEST(EncodingCacheTest, ChangeThatWouldExceedTheBudgetKeepsThePrevious) {
  std::vector<Phrase> phrases;
  phrases.reserve(kPhromBudget + 1);
  for (int i = 0; i < kPhromBudget; ++i) {
    phrases.push_back(
        LockedPhrase(i + 1, "P" + std::to_string(i), kPhromWordCapacity - 100));
  }
  phrases.push_back(CurrentPhrase(100, "OPEN"));
  Project project = ProjectWith(std::move(phrases));
  ASSERT_EQ(project.Allocation().PhromsUsed(), 64);
  const Project before = project;

  MockEncoder encoder;
  EncodingCache cache(encoder);
  EXPECT_CALL(encoder, Encode(_, _, _)).WillOnce(Return(ResultPackingTo(200)));
  const StatusOr<EncodingChange> result =
      cache.ChangeSettings(project, TestId(100), {{"target_rms", "30"}});
  ASSERT_FALSE(result.IsOk());
  EXPECT_EQ(result.GetStatus().Code(), StatusCode::kResourceExhausted);
  const std::string& message = result.GetStatus().Message();
  EXPECT_NE(message.find("not applied"), std::string::npos) << message;
  EXPECT_NE(message.find("65 PHROMs"), std::string::npos) << message;
  EXPECT_NE(message.find("keeps its previous settings"), std::string::npos)
      << message;
  EXPECT_EQ(project, before);
}

TEST(EncodingCacheTest, LockedPhraseIsNeverReencoded) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  Project project = ProjectWith({LockedPhrase(1, "L", 10)});
  EXPECT_CALL(encoder, Encode(_, _, _)).Times(0);
  EXPECT_EQ(cache.ChangeSettings(project, TestId(1), {}).GetStatus().Code(),
            StatusCode::kFailedPrecondition);
  const RefreshReport report = cache.Refresh(project, /*force=*/true);
  EXPECT_EQ(report.encoded, 0);
  EXPECT_EQ(report.skipped, 1);
}

TEST(EncodingCacheTest, RefreshEncodesOnlyStalePhrases) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  Project project =
      ProjectWith({CurrentPhrase(1, "CURRENT"), AudioPhrase(2, "STALE")});
  EXPECT_CALL(encoder, Encode(_, _, _)).WillOnce(Return(ResultPackingTo(7)));
  const RefreshReport report = cache.Refresh(project);
  EXPECT_EQ(report.encoded, 1);
  EXPECT_EQ(report.skipped, 1);
  EXPECT_TRUE(report.failures.empty());
  EXPECT_EQ(project.FindPhrase(TestId(2))->encoding->frames,
            FramesPackingTo(7));
}

TEST(EncodingCacheTest, ForcedRefreshEncodesEveryUnlockedPhrase) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  Project project =
      ProjectWith({CurrentPhrase(1, "CURRENT"), AudioPhrase(2, "STALE"),
                   LockedPhrase(3, "LOCKED", 10)});
  EXPECT_CALL(encoder, Encode(_, _, _))
      .Times(2)
      .WillRepeatedly(Return(ResultPackingTo(7)));
  const RefreshReport report = cache.Refresh(project, /*force=*/true);
  EXPECT_EQ(report.encoded, 2);
  EXPECT_EQ(report.skipped, 1);
}

TEST(EncodingCacheTest, RefreshGoesOnAfterAFailure) {
  MockEncoder encoder;
  EncodingCache cache(encoder);
  Project project =
      ProjectWith({AudioPhrase(1, "BAD"), AudioPhrase(2, "GOOD")});
  EXPECT_CALL(encoder, Encode(_, _, _))
      .WillOnce(Return(InvalidArgumentError("broken")))
      .WillOnce(Return(ResultPackingTo(7)));
  const RefreshReport report = cache.Refresh(project);
  EXPECT_EQ(report.encoded, 1);
  ASSERT_EQ(report.failures.size(), 1U);
  EXPECT_EQ(report.failures[0].id, TestId(1));
  EXPECT_FALSE(project.FindPhrase(TestId(1))->encoding.has_value());
  EXPECT_TRUE(project.FindPhrase(TestId(2))->encoding.has_value());
}

TEST(EncodingCacheTest, RefreshChecksTheBudgetPhraseByPhrase) {
  std::vector<Phrase> phrases;
  phrases.reserve(kPhromBudget + 1);
  for (int i = 0; i < kPhromBudget - 1; ++i) {
    phrases.push_back(
        LockedPhrase(i + 1, "P" + std::to_string(i), kPhromWordCapacity));
  }
  phrases.push_back(AudioPhrase(101, "FIRST"));
  phrases.push_back(AudioPhrase(102, "SECOND"));
  Project project = ProjectWith(std::move(phrases));
  MockEncoder encoder;
  EncodingCache cache(encoder);
  // FIRST fills PHROM 64; SECOND would need a 65th.
  EXPECT_CALL(encoder, Encode(_, _, _))
      .WillOnce(Return(ResultPackingTo(16000)))
      .WillOnce(Return(ResultPackingTo(1000)));
  const RefreshReport report = cache.Refresh(project);
  EXPECT_EQ(report.encoded, 1);
  ASSERT_EQ(report.failures.size(), 1U);
  EXPECT_EQ(report.failures[0].id, TestId(102));
  EXPECT_EQ(report.failures[0].status.Code(), StatusCode::kResourceExhausted);
  EXPECT_EQ(project.Allocation().PhromsUsed(), 64);
}

}  // namespace
}  // namespace speak5220::core::project
