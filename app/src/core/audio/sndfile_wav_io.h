/*
 * File:        sndfile_wav_io.h
 * Module:      audio
 * Purpose:     WAV reading and writing over libsndfile
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#ifndef SPEAK5220_CORE_AUDIO_SNDFILE_WAV_IO_H_
#define SPEAK5220_CORE_AUDIO_SNDFILE_WAV_IO_H_

#include <cstdint>
#include <filesystem>
#include <span>

#include "core/audio/wav_io.h"
#include "core/util/status.h"

namespace speak5220::core::audio {

// IWavReader over libsndfile. Accepts WAV only (plain RIFF and
// WAVE_FORMAT_EXTENSIBLE). Stateless; thread-safe.
class SndfileWavReader : public IWavReader {
 public:
  // Inputs larger than this are refused, rather than read into memory:
  // ten minutes at 192 kHz, far beyond any phrase.
  static constexpr std::int64_t kMaxFrames = std::int64_t{192000} * 600;
  static constexpr int kMaxChannels = 64;

  StatusOr<DecodedWav> Read(const std::filesystem::path& path) override;
};

// IWavWriter over libsndfile. Stateless; thread-safe.
class SndfileWavWriter : public IWavWriter {
 public:
  Status Write(const std::filesystem::path& path,
               std::span<const float> samples, int sample_rate,
               WavOutputFormat format) override;
};

}  // namespace speak5220::core::audio

#endif  // SPEAK5220_CORE_AUDIO_SNDFILE_WAV_IO_H_
