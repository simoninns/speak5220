/*
 * File:        phrase_importer_test.cpp
 * Module:      project tests
 * Purpose:     Unit tests of the import pipeline with a mocked WAV reader,
 *              resampler and file digest
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/project/phrase_importer.h"

#include <filesystem>
#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/pcm24.h"
#include "core/audio/wav_io.h"
#include "core/project/phrase.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"
#include "support/project_mocks.h"

namespace speak5220::core::project {
namespace {

using ::testing::_;
using ::testing::ElementsAre;
using testing::MockFileDigest;
using testing::MockResampler;
using testing::MockWavReader;
using ::testing::Return;
using testing::TestId;

const std::string kDigest(64, 'a');

audio::DecodedWav Wav(int rate, int channels, std::vector<float> samples) {
  return {.sample_rate = rate,
          .channels = channels,
          .format = audio::WavSampleFormat::kPcm16,
          .samples = std::move(samples)};
}

class PhraseImporterTest : public ::testing::Test {
 protected:
  PhraseImporterTest() {
    ON_CALL(digest_, Sha256(_)).WillByDefault(Return(kDigest));
  }

  StatusOr<ImportResult> Import(const ImportRequest& request,
                                const std::set<std::string>& taken = {}) {
    return importer_.Import(request, TestId(1), taken);
  }

  MockWavReader reader_;
  MockResampler resampler_;
  ::testing::NiceMock<MockFileDigest> digest_;
  PhraseImporter importer_{reader_, resampler_, digest_};
};

TEST_F(PhraseImporterTest, ResamplesOtherRatesTo8kHz) {
  const std::filesystem::path path = "dir/hello_world.wav";
  EXPECT_CALL(reader_, Read(path))
      .WillOnce(Return(Wav(44100, 2, std::vector<float>(441, 0.25F))));
  EXPECT_CALL(resampler_, Resample(_, 44100, kProjectSampleRate))
      .WillOnce(Return(std::vector<float>(80, 0.25F)));
  const StatusOr<ImportResult> result = Import({.path = path});
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  const Phrase& phrase = result->phrase;
  EXPECT_EQ(phrase.id, TestId(1));
  EXPECT_EQ(phrase.name, "HELLO WORLD");
  EXPECT_EQ(phrase.audio, std::vector<float>(80, 0.25F));
  ASSERT_TRUE(phrase.source.has_value());
  EXPECT_EQ(phrase.source->file, "hello_world.wav");
  EXPECT_EQ(phrase.source->rate, 44100);
  EXPECT_EQ(phrase.source->channels, 2);
  EXPECT_DOUBLE_EQ(phrase.source->duration, 0.01);
  EXPECT_EQ(phrase.source->sha256, kDigest);
  EXPECT_FALSE(phrase.locked);
  EXPECT_FALSE(phrase.encoding.has_value());
}

TEST_F(PhraseImporterTest, CopiesAudioThatIsAlready8kHz) {
  EXPECT_CALL(reader_, Read(_))
      .WillOnce(Return(Wav(8000, 1, {0.1F, -0.2F, 0.3F})));
  EXPECT_CALL(resampler_, Resample(_, _, _)).Times(0);
  const StatusOr<ImportResult> result = Import({.path = "a.wav"});
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(result->phrase.audio.size(), 3U);
}

TEST_F(PhraseImporterTest, MovesSamplesToThe24BitGrid) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.1F, 1.5F})));
  const StatusOr<ImportResult> result = Import({.path = "a.wav"});
  ASSERT_TRUE(result.IsOk());
  for (const float sample : result->phrase.audio) {
    EXPECT_EQ(audio::FromPcm24(audio::ToPcm24(sample)), sample);
  }
  EXPECT_EQ(result->phrase.audio[1], audio::FromPcm24(audio::kPcm24Max));
}

TEST_F(PhraseImporterTest, ReportsClippingWithoutChangingTheLevel) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.5F, -1.0F})));
  const StatusOr<ImportResult> result = Import({.path = "a.wav"});
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(result->clipped);
  EXPECT_FALSE(result->quiet);
  EXPECT_FALSE(result->normalised);
  EXPECT_EQ(result->phrase.audio[0], 0.5F);
}

TEST_F(PhraseImporterTest, NormalisesClippingInputWhenAsked) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.5F, -1.0F})));
  const StatusOr<ImportResult> result =
      Import({.path = "a.wav", .normalise = true});
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(result->normalised);
  EXPECT_NEAR(result->phrase.audio[1], -kNormalisedPeak, 1e-6);
  EXPECT_NEAR(result->phrase.audio[0], kNormalisedPeak / 2, 1e-6);
}

TEST_F(PhraseImporterTest, NormalisesVeryQuietInputWhenAsked) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.01F, 0.0F})));
  const StatusOr<ImportResult> result =
      Import({.path = "a.wav", .normalise = true});
  ASSERT_TRUE(result.IsOk());
  EXPECT_TRUE(result->quiet);
  EXPECT_TRUE(result->normalised);
  EXPECT_NEAR(result->phrase.audio[0], kNormalisedPeak, 1e-6);
}

TEST_F(PhraseImporterTest, NeverNormalisesOrdinaryInput) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.5F, -0.25F})));
  const StatusOr<ImportResult> result =
      Import({.path = "a.wav", .normalise = true});
  ASSERT_TRUE(result.IsOk());
  EXPECT_FALSE(result->normalised);
  EXPECT_THAT(result->phrase.audio, ElementsAre(0.5F, -0.25F));
}

TEST_F(PhraseImporterTest, UsesARequestedName) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.5F})));
  const StatusOr<ImportResult> result =
      Import({.path = "a.wav", .name = "good-bye"});
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(result->phrase.name, "GOOD-BYE");
}

TEST_F(PhraseImporterTest, RefusesAnInvalidOrTakenName) {
  EXPECT_CALL(reader_, Read(_)).Times(0);
  EXPECT_FALSE(Import({.path = "a.wav", .name = "bad!"}).IsOk());
  EXPECT_FALSE(Import({.path = "a.wav", .name = "hello"}, {"HELLO"}).IsOk());
}

TEST_F(PhraseImporterTest, MadeUpNameAvoidsTakenNames) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.5F})));
  const StatusOr<ImportResult> result =
      Import({.path = "hello.wav"}, {"HELLO"});
  ASSERT_TRUE(result.IsOk());
  EXPECT_EQ(result->phrase.name, "HELLO 2");
}

TEST_F(PhraseImporterTest, PassesOnReadResampleAndDigestFailures) {
  EXPECT_CALL(reader_, Read(_))
      .WillOnce(Return(NotFoundError("cannot open a.wav")))
      .WillOnce(Return(Wav(44100, 1, {0.5F})))
      .WillOnce(Return(Wav(8000, 1, {0.5F})));
  EXPECT_EQ(Import({.path = "a.wav"}).GetStatus().Message(),
            "cannot open a.wav");
  EXPECT_CALL(resampler_, Resample(_, _, _))
      .WillOnce(Return(InternalError("resampler failed")));
  EXPECT_EQ(Import({.path = "a.wav"}).GetStatus().Message(),
            "resampler failed");
  EXPECT_CALL(digest_, Sha256(_))
      .WillOnce(Return(UnavailableError("cannot read")));
  EXPECT_EQ(Import({.path = "a.wav"}).GetStatus().Message(), "cannot read");
}

TEST_F(PhraseImporterTest, RunsAsABackgroundJob) {
  EXPECT_CALL(reader_, Read(_)).WillOnce(Return(Wav(8000, 1, {0.5F})));
  InlineTaskRunner runner;
  std::optional<StatusOr<ImportResult>> delivered;
  importer_.ImportInBackground(runner, {.path = "hello.wav"}, TestId(7), {},
                               [&delivered](StatusOr<ImportResult> result) {
                                 delivered.emplace(std::move(result));
                               });
  ASSERT_TRUE(delivered.has_value());
  ASSERT_TRUE(delivered->IsOk());
  EXPECT_EQ((*delivered)->phrase.id, TestId(7));
  EXPECT_EQ((*delivered)->phrase.name, "HELLO");
}

}  // namespace
}  // namespace speak5220::core::project
