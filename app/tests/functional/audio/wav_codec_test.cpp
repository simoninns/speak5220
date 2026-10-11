/*
 * File:        wav_codec_test.cpp
 * Module:      audio tests
 * Purpose:     Functional tests of the in-memory WAV codec over libsndfile:
 *              exact 24-bit round trips and refusals
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include <QTemporaryDir>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <vector>

#include "core/audio/pcm24.h"
#include "core/audio/sndfile_wav_io.h"
#include "core/audio/wav_io.h"
#include "core/util/status.h"
#include "gtest/gtest.h"

namespace speak5220::core::audio {
namespace {

std::vector<float> GridSamples() {
  // Codes across the whole range, full scale included.
  std::vector<float> samples;
  for (const int code : {0, 1, -1, 4194304, -4194305, kPcm24Max, kPcm24Min,
                         8388000, -8388000, 123457}) {
    samples.push_back(FromPcm24(code));
  }
  return samples;
}

TEST(WavCodecTest, TwentyFourBitRoundTripIsExact) {
  SndfileWavCodec codec;
  const std::vector<float> samples = GridSamples();
  const StatusOr<std::vector<std::uint8_t>> bytes =
      codec.Encode(samples, 8000, WavOutputFormat::kPcm24);
  ASSERT_TRUE(bytes.IsOk()) << bytes.GetStatus().ToString();
  // A 44-byte RIFF header and 3 bytes per sample.
  EXPECT_EQ(bytes->size(), 44 + 3 * samples.size());
  const StatusOr<DecodedWav> decoded = codec.Decode(*bytes);
  ASSERT_TRUE(decoded.IsOk()) << decoded.GetStatus().ToString();
  EXPECT_EQ(decoded->sample_rate, 8000);
  EXPECT_EQ(decoded->channels, 1);
  EXPECT_EQ(decoded->format, WavSampleFormat::kPcm24);
  EXPECT_EQ(decoded->samples, samples);
}

TEST(WavCodecTest, SamplesOffTheGridAreRoundedToTheNearestCode) {
  SndfileWavCodec codec;
  const std::vector<float> samples = {0.1F, -0.7F, 1.5F};
  const StatusOr<DecodedWav> decoded =
      codec.Decode(*codec.Encode(samples, 8000, WavOutputFormat::kPcm24));
  ASSERT_TRUE(decoded.IsOk());
  for (std::size_t i = 0; i < samples.size(); ++i) {
    EXPECT_EQ(decoded->samples[i], FromPcm24(ToPcm24(samples[i])));
  }
}

TEST(WavCodecTest, SixteenBitOutputKeepsSixteenBits) {
  SndfileWavCodec codec;
  const std::vector<float> samples = {0.5F, -0.25F, 1.0F / 32768.0F};
  const StatusOr<std::vector<std::uint8_t>> bytes =
      codec.Encode(samples, 8000, WavOutputFormat::kPcm16);
  ASSERT_TRUE(bytes.IsOk());
  EXPECT_EQ(bytes->size(), 44 + 2 * samples.size());
  const StatusOr<DecodedWav> decoded = codec.Decode(*bytes);
  ASSERT_TRUE(decoded.IsOk());
  EXPECT_EQ(decoded->format, WavSampleFormat::kPcm16);
  EXPECT_EQ(decoded->samples, samples);
}

TEST(WavCodecTest, DecodesWhatTheFileReaderReads) {
  QTemporaryDir directory;
  ASSERT_TRUE(directory.isValid());
  const std::filesystem::path path =
      std::filesystem::path(directory.path().toStdString()) / "tone.wav";
  const std::vector<float> samples = {0.25F, -0.5F, 0.125F, 0.0F};
  ASSERT_TRUE(SndfileWavWriter()
                  .Write(path, samples, 11025, WavOutputFormat::kPcm16)
                  .IsOk());
  std::ifstream file(path, std::ios::binary);
  const std::string text((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
  const std::vector<std::uint8_t> bytes(text.begin(), text.end());
  const StatusOr<DecodedWav> decoded = SndfileWavCodec().Decode(bytes);
  const StatusOr<DecodedWav> read = SndfileWavReader().Read(path);
  ASSERT_TRUE(decoded.IsOk());
  ASSERT_TRUE(read.IsOk());
  EXPECT_EQ(decoded->sample_rate, 11025);
  EXPECT_EQ(decoded->samples, read->samples);
}

TEST(WavCodecTest, RefusesBytesThatAreNotAWavFile) {
  SndfileWavCodec codec;
  EXPECT_FALSE(codec.Decode({}).IsOk());
  const std::vector<std::uint8_t> junk(100, 0x55);
  EXPECT_FALSE(codec.Decode(junk).IsOk());
  // A truncated file: the header of a real one.
  const std::vector<std::uint8_t> whole =
      *codec.Encode(GridSamples(), 8000, WavOutputFormat::kPcm24);
  const std::vector<std::uint8_t> header(whole.begin(), whole.begin() + 20);
  EXPECT_FALSE(codec.Decode(header).IsOk());
}

TEST(WavCodecTest, RefusesABadRate) {
  const std::vector<float> samples = {0.0F};
  EXPECT_FALSE(
      SndfileWavCodec().Encode(samples, 0, WavOutputFormat::kPcm24).IsOk());
}

}  // namespace
}  // namespace speak5220::core::audio
