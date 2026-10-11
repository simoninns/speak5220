/*
 * File:        project_mocks.h
 * Module:      test support
 * Purpose:     gMock mocks of the interfaces the project code depends on:
 *              the encoder, WAV reading and coding, resampling, file
 *              digests and project stores
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_TESTS_SUPPORT_PROJECT_MOCKS_H_
#define SPEAK5220_TESTS_SUPPORT_PROJECT_MOCKS_H_

#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include "core/audio/wav_io.h"
#include "core/dsp/resampler.h"
#include "core/encoder/encoder.h"
#include "core/encoder/encoder_options.h"
#include "core/project/project_store.h"
#include "core/util/file_digest.h"
#include "core/util/status.h"
#include "gmock/gmock.h"

namespace speak5220::testing {

class MockEncoder : public core::encoder::IEncoder {
 public:
  MOCK_METHOD(core::StatusOr<core::encoder::EncodeResult>, Encode,
              (std::span<const double> samples, int sample_rate,
               const core::encoder::EncoderOptions& options),
              (override));
};

class MockWavReader : public core::audio::IWavReader {
 public:
  MOCK_METHOD(core::StatusOr<core::audio::DecodedWav>, Read,
              (const std::filesystem::path& path), (override));
};

class MockWavCodec : public core::audio::IWavCodec {
 public:
  MOCK_METHOD(core::StatusOr<core::audio::DecodedWav>, Decode,
              (std::span<const std::uint8_t> bytes), (override));
  MOCK_METHOD(core::StatusOr<std::vector<std::uint8_t>>, Encode,
              (std::span<const float> samples, int sample_rate,
               core::audio::WavOutputFormat format),
              (override));
};

class MockResampler : public core::dsp::IResampler {
 public:
  MOCK_METHOD(core::StatusOr<std::vector<float>>, Resample,
              (std::span<const float> input, int input_rate, int output_rate),
              (override));
};

class MockFileDigest : public core::IFileDigest {
 public:
  MOCK_METHOD(core::StatusOr<std::string>, Sha256,
              (const std::filesystem::path& path), (override));
};

class MockProjectStore : public core::project::IProjectStore {
 public:
  MOCK_METHOD(core::StatusOr<core::project::ProjectFiles>, Read, (),
              (override));
  MOCK_METHOD(core::Status, Write, (const core::project::ProjectFiles& files),
              (override));
};

}  // namespace speak5220::testing

#endif  // SPEAK5220_TESTS_SUPPORT_PROJECT_MOCKS_H_
