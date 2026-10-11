/*
 * File:        phrase_import_test.cpp
 * Module:      project tests
 * Purpose:     Functional tests of the import pipeline with the real WAV
 *              reader, resampler and file digest
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <sndfile.h>

#include <QTemporaryDir>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <numbers>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/pcm24.h"
#include "core/audio/sndfile_wav_io.h"
#include "core/dsp/resampler.h"
#include "core/project/phrase.h"
#include "core/project/phrase_importer.h"
#include "core/util/file_digest.h"
#include "core/util/status.h"
#include "core/util/task_runner.h"
#include "gtest/gtest.h"
#include "support/project_builder.h"

namespace speak5220::core::project {
namespace {

namespace fs = std::filesystem;

// A 300 Hz tone at -6 dBFS, `seconds` long, with a phase offset per
// channel, written with libsndfile in `format`.
void WriteTone(const fs::path& path, int rate, int channels, int format,
               double seconds) {
  SF_INFO info = {};
  info.samplerate = rate;
  info.channels = channels;
  info.format = format;
  SNDFILE* file = sf_open(path.string().c_str(), SFM_WRITE, &info);
  ASSERT_NE(file, nullptr) << sf_strerror(nullptr);
  const auto frames = static_cast<std::size_t>(seconds * rate);
  std::vector<float> data(frames * static_cast<std::size_t>(channels));
  for (std::size_t i = 0; i < frames; ++i) {
    for (int c = 0; c < channels; ++c) {
      data[i * static_cast<std::size_t>(channels) +
           static_cast<std::size_t>(c)] =
          0.5F * static_cast<float>(std::sin(2.0 * std::numbers::pi * 300.0 *
                                                 static_cast<double>(i) / rate +
                                             0.1 * c));
    }
  }
  EXPECT_EQ(sf_writef_float(file, data.data(), static_cast<sf_count_t>(frames)),
            static_cast<sf_count_t>(frames));
  sf_close(file);
}

class PhraseImportTest : public ::testing::Test {
 protected:
  void SetUp() override { ASSERT_TRUE(directory_.isValid()); }

  fs::path Path(const std::string& name) const {
    return fs::path(directory_.path().toStdString()) / name;
  }

  QTemporaryDir directory_;
  audio::SndfileWavReader reader_;
  dsp::SincResampler resampler_;
  FileSha256Digest digest_;
  PhraseImporter importer_{reader_, resampler_, digest_};
};

TEST_F(PhraseImportTest, Imports44kHzStereo) {
  const fs::path path = Path("good_morning.wav");
  WriteTone(path, 44100, 2, SF_FORMAT_WAV | SF_FORMAT_PCM_16, 0.5);
  const StatusOr<ImportResult> result =
      importer_.Import({.path = path}, testing::TestId(1), {});
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  const Phrase& phrase = result->phrase;
  EXPECT_EQ(phrase.name, "GOOD MORNING");
  ASSERT_TRUE(phrase.source.has_value());
  EXPECT_EQ(phrase.source->file, "good_morning.wav");
  EXPECT_EQ(phrase.source->rate, 44100);
  EXPECT_EQ(phrase.source->channels, 2);
  EXPECT_DOUBLE_EQ(phrase.source->duration, 0.5);
  EXPECT_EQ(phrase.source->sha256, *digest_.Sha256(path));
  // Half a second at 8 kHz, at the tone's level.
  EXPECT_NEAR(static_cast<double>(phrase.audio.size()), 4000.0, 2.0);
  float peak = 0.0F;
  for (const float sample : phrase.audio) {
    peak = std::max(peak, std::abs(sample));
    EXPECT_EQ(audio::FromPcm24(audio::ToPcm24(sample)), sample);
  }
  EXPECT_NEAR(peak, 0.5F, 0.02F);
  EXPECT_FALSE(result->clipped);
  EXPECT_FALSE(result->quiet);
}

TEST_F(PhraseImportTest, Imports8kHzMonoUnchanged) {
  const fs::path path = Path("hello.wav");
  WriteTone(path, 8000, 1, SF_FORMAT_WAV | SF_FORMAT_PCM_24, 0.25);
  const StatusOr<ImportResult> result =
      importer_.Import({.path = path}, testing::TestId(2), {});
  ASSERT_TRUE(result.IsOk()) << result.GetStatus().ToString();
  const StatusOr<audio::DecodedWav> original = reader_.Read(path);
  ASSERT_TRUE(original.IsOk());
  EXPECT_EQ(result->phrase.audio, original->samples);
  EXPECT_EQ(result->phrase.source->rate, 8000);
  EXPECT_EQ(result->phrase.source->channels, 1);
}

TEST_F(PhraseImportTest, ImportsOnAThreadPool) {
  const fs::path path = Path("hello.wav");
  WriteTone(path, 16000, 1, SF_FORMAT_WAV | SF_FORMAT_FLOAT, 0.1);
  ThreadPoolTaskRunner runner(2);
  std::optional<StatusOr<ImportResult>> delivered;
  importer_.ImportInBackground(runner, {.path = path}, testing::TestId(3), {},
                               [&delivered](StatusOr<ImportResult> result) {
                                 delivered.emplace(std::move(result));
                               });
  runner.Wait();
  ASSERT_TRUE(delivered.has_value());
  ASSERT_TRUE(delivered->IsOk()) << delivered->GetStatus().ToString();
  EXPECT_NEAR(static_cast<double>((*delivered)->phrase.audio.size()), 800.0,
              2.0);
}

TEST_F(PhraseImportTest, MissingOrBrokenFileIsReported) {
  EXPECT_FALSE(
      importer_.Import({.path = Path("none.wav")}, testing::TestId(4), {})
          .IsOk());
  std::ofstream(Path("junk.wav"), std::ios::binary) << "RIFF but not really";
  EXPECT_FALSE(
      importer_.Import({.path = Path("junk.wav")}, testing::TestId(4), {})
          .IsOk());
}

}  // namespace
}  // namespace speak5220::core::project
