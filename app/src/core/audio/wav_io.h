/*
 * File:        wav_io.h
 * Module:      audio
 * Purpose:     Interfaces for reading and writing WAV files
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_AUDIO_WAV_IO_H_
#define SPEAK5220_CORE_AUDIO_WAV_IO_H_

#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>

#include "core/util/status.h"

namespace speak5220::core::audio {

// The sample formats a WAV file may hold.
enum class WavSampleFormat { kPcm8, kPcm16, kPcm24, kPcm32, kFloat, kDouble };

// A WAV file as read: mixed to mono, as floats in -1..1.
struct DecodedWav {
  int sample_rate = 0;
  int channels = 0;  // in the file, before mixing
  WavSampleFormat format = WavSampleFormat::kPcm16;
  std::vector<float> samples;
};

// The formats the application writes.
enum class WavOutputFormat { kPcm16, kPcm24 };

// Reads WAV files. Implementations are thread-safe.
class IWavReader {
 public:
  virtual ~IWavReader() = default;

  // Reads any WAV variant (PCM 8-32-bit, float, WAVE_FORMAT_EXTENSIBLE),
  // any rate and channel count, mixing the channels to mono by averaging.
  // Fails for a file that is missing, not WAV, empty or malformed.
  virtual StatusOr<DecodedWav> Read(const std::filesystem::path& path) = 0;
};

// Writes WAV files. Implementations are thread-safe.
class IWavWriter {
 public:
  virtual ~IWavWriter() = default;

  // Writes mono `samples` (-1..1; values outside are clipped) at
  // `sample_rate`, replacing any existing file.
  virtual Status Write(const std::filesystem::path& path,
                       std::span<const float> samples, int sample_rate,
                       WavOutputFormat format) = 0;
};

// Converts between samples and the bytes of a WAV file held in memory, as
// a project stores its audio. Implementations are thread-safe.
class IWavCodec {
 public:
  virtual ~IWavCodec() = default;

  // Decodes a WAV file's bytes as IWavReader::Read() reads a file. 24-bit
  // and 16-bit PCM decode exactly (code / 2^23, code / 2^15). Fails for
  // bytes that are not a usable WAV file.
  virtual StatusOr<DecodedWav> Decode(std::span<const std::uint8_t> bytes) = 0;

  // Encodes mono `samples` (-1..1) as a WAV file at `sample_rate`. For
  // 24-bit PCM each sample is rounded to the nearest code (ToPcm24()), so a
  // sample already on the 24-bit grid decodes back exactly.
  virtual StatusOr<std::vector<std::uint8_t>> Encode(
      std::span<const float> samples, int sample_rate,
      WavOutputFormat format) = 0;
};

}  // namespace speak5220::core::audio

#endif  // SPEAK5220_CORE_AUDIO_WAV_IO_H_
