/*
 * File:        wav_io_test.cpp
 * Module:      audio tests
 * Purpose:     Functional tests of the libsndfile WAV adapters: every
 *              supported format, round trips and refusals
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/audio/wav_io.h"

#include <sndfile.h>

#include <QTemporaryDir>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <numbers>
#include <string>
#include <vector>

#include "core/audio/sndfile_wav_io.h"
#include "gtest/gtest.h"

namespace speak5220::core::audio {
namespace {

constexpr int kRate = 22050;
constexpr std::size_t kFrames = 2205;

// A 440 Hz tone at -6 dBFS, with a phase offset per channel.
float Tone(std::size_t i, int channel) {
  return 0.5F * static_cast<float>(std::sin(2.0 * std::numbers::pi * 440.0 *
                                                static_cast<double>(i) / kRate +
                                            channel));
}

// Writes a file directly with libsndfile, so the reader is tested against
// files it did not write.
void WriteWithSndfile(const std::filesystem::path& path, int format,
                      int channels) {
  SF_INFO info = {};
  info.samplerate = kRate;
  info.channels = channels;
  info.format = format;
  SNDFILE* file = sf_open(path.string().c_str(), SFM_WRITE, &info);
  ASSERT_NE(file, nullptr) << sf_strerror(nullptr);
  std::vector<float> data(kFrames * static_cast<std::size_t>(channels));
  for (std::size_t i = 0; i < kFrames; ++i) {
    for (int c = 0; c < channels; ++c) {
      data[i * static_cast<std::size_t>(channels) +
           static_cast<std::size_t>(c)] = Tone(i, c);
    }
  }
  EXPECT_EQ(sf_writef_float(file, data.data(), kFrames),
            static_cast<sf_count_t>(kFrames));
  sf_close(file);
}

struct FormatCase {
  const char* name;
  int format;
  int channels;
  WavSampleFormat expected;
  float tolerance;
};

class WavReaderFormatTest : public ::testing::TestWithParam<FormatCase> {};

TEST_P(WavReaderFormatTest, ReadsFormatAndMixesToMono) {
  const FormatCase& c = GetParam();
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const std::filesystem::path path =
      dir.filePath(QStringLiteral("in.wav")).toStdString();
  WriteWithSndfile(path, c.format, c.channels);
  SndfileWavReader reader;
  const StatusOr<DecodedWav> wav = reader.Read(path);
  ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
  EXPECT_EQ(wav->sample_rate, kRate);
  EXPECT_EQ(wav->channels, c.channels);
  EXPECT_EQ(wav->format, c.expected);
  ASSERT_EQ(wav->samples.size(), kFrames);
  for (std::size_t i = 0; i < kFrames; ++i) {
    float mono = 0.0F;
    for (int ch = 0; ch < c.channels; ++ch) {
      mono += Tone(i, ch);
    }
    mono /= static_cast<float>(c.channels);
    ASSERT_NEAR(wav->samples[i], mono, c.tolerance) << "sample " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(
    EveryWavVariant, WavReaderFormatTest,
    ::testing::Values(FormatCase{"Pcm8", SF_FORMAT_WAV | SF_FORMAT_PCM_U8, 1,
                                 WavSampleFormat::kPcm8, 1.0F / 64},
                      FormatCase{"Pcm16", SF_FORMAT_WAV | SF_FORMAT_PCM_16, 1,
                                 WavSampleFormat::kPcm16, 1.0F / 16384},
                      FormatCase{"Pcm24", SF_FORMAT_WAV | SF_FORMAT_PCM_24, 1,
                                 WavSampleFormat::kPcm24, 1e-6F},
                      FormatCase{"Pcm32", SF_FORMAT_WAV | SF_FORMAT_PCM_32, 1,
                                 WavSampleFormat::kPcm32, 1e-6F},
                      FormatCase{"Float", SF_FORMAT_WAV | SF_FORMAT_FLOAT, 1,
                                 WavSampleFormat::kFloat, 1e-7F},
                      FormatCase{"Double", SF_FORMAT_WAV | SF_FORMAT_DOUBLE, 1,
                                 WavSampleFormat::kDouble, 1e-7F},
                      FormatCase{"Stereo16", SF_FORMAT_WAV | SF_FORMAT_PCM_16,
                                 2, WavSampleFormat::kPcm16, 1.0F / 16384},
                      FormatCase{"Extensible",
                                 SF_FORMAT_WAVEX | SF_FORMAT_PCM_24, 6,
                                 WavSampleFormat::kPcm24, 1e-6F}),
    [](const ::testing::TestParamInfo<FormatCase>& param_info) {
      return std::string(param_info.param.name);
    });

class WavRoundTripTest : public ::testing::TestWithParam<WavOutputFormat> {};

TEST_P(WavRoundTripTest, WritesAndReadsBackWithinOneStep) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const std::filesystem::path path =
      dir.filePath(QStringLiteral("out.wav")).toStdString();
  std::vector<float> samples(kFrames);
  for (std::size_t i = 0; i < kFrames; ++i) {
    samples[i] = Tone(i, 0);
  }
  SndfileWavWriter writer;
  ASSERT_TRUE(writer.Write(path, samples, 8000, GetParam()).IsOk());
  SndfileWavReader reader;
  const StatusOr<DecodedWav> wav = reader.Read(path);
  ASSERT_TRUE(wav.IsOk()) << wav.GetStatus().ToString();
  const bool is24 = GetParam() == WavOutputFormat::kPcm24;
  EXPECT_EQ(wav->format,
            is24 ? WavSampleFormat::kPcm24 : WavSampleFormat::kPcm16);
  EXPECT_EQ(wav->sample_rate, 8000);
  ASSERT_EQ(wav->samples.size(), samples.size());
  const float step = is24 ? 1.0F / 8388608 : 1.0F / 32768;
  for (std::size_t i = 0; i < samples.size(); ++i) {
    ASSERT_NEAR(wav->samples[i], samples[i], step) << "sample " << i;
  }
}

INSTANTIATE_TEST_SUITE_P(BothOutputFormats, WavRoundTripTest,
                         ::testing::Values(WavOutputFormat::kPcm16,
                                           WavOutputFormat::kPcm24));

TEST(WavWriterTest, ClipsOutOfRangeSamples) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const std::filesystem::path path =
      dir.filePath(QStringLiteral("clip.wav")).toStdString();
  const std::vector<float> samples = {2.0F, -2.0F};
  SndfileWavWriter writer;
  ASSERT_TRUE(
      writer.Write(path, samples, 8000, WavOutputFormat::kPcm16).IsOk());
  const StatusOr<DecodedWav> wav = SndfileWavReader().Read(path);
  ASSERT_TRUE(wav.IsOk());
  EXPECT_NEAR(wav->samples[0], 1.0F, 1e-4F);
  EXPECT_NEAR(wav->samples[1], -1.0F, 1e-4F);
}

TEST(WavReaderTest, RefusesMissingFile) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const StatusOr<DecodedWav> wav = SndfileWavReader().Read(
      dir.filePath(QStringLiteral("none.wav")).toStdString());
  ASSERT_FALSE(wav.IsOk());
  EXPECT_EQ(wav.GetStatus().Code(), StatusCode::kNotFound);
}

TEST(WavReaderTest, RefusesFilesThatAreNotWav) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const std::filesystem::path path =
      dir.filePath(QStringLiteral("in.aiff")).toStdString();
  WriteWithSndfile(path, SF_FORMAT_AIFF | SF_FORMAT_PCM_16, 1);
  const StatusOr<DecodedWav> wav = SndfileWavReader().Read(path);
  ASSERT_FALSE(wav.IsOk());
  EXPECT_EQ(wav.GetStatus().Code(), StatusCode::kInvalidArgument);
}

TEST(WavReaderTest, RefusesUnsupportedWavCodec) {
  QTemporaryDir dir;
  ASSERT_TRUE(dir.isValid());
  const std::filesystem::path path =
      dir.filePath(QStringLiteral("ulaw.wav")).toStdString();
  WriteWithSndfile(path, SF_FORMAT_WAV | SF_FORMAT_ULAW, 1);
  const StatusOr<DecodedWav> wav = SndfileWavReader().Read(path);
  ASSERT_FALSE(wav.IsOk());
  EXPECT_EQ(wav.GetStatus().Code(), StatusCode::kInvalidArgument);
}

}  // namespace
}  // namespace speak5220::core::audio
