/*
 * File:        sndfile_wav_io.cpp
 * Module:      audio
 * Purpose:     WAV reading and writing over libsndfile
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 * SPDX-FileCopyrightText: (c) 2026 Simon Inns
 */

#include "core/audio/sndfile_wav_io.h"

#include <sndfile.h>

#include <cstddef>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "core/util/logging.h"

namespace speak5220::core::audio {
namespace {

struct SndfileCloser {
  void operator()(SNDFILE* file) const { sf_close(file); }
};
using SndfileHandle = std::unique_ptr<SNDFILE, SndfileCloser>;

std::optional<WavSampleFormat> SampleFormatOf(int format) {
  switch (format & SF_FORMAT_SUBMASK) {
    case SF_FORMAT_PCM_U8:
    case SF_FORMAT_PCM_S8:
      return WavSampleFormat::kPcm8;
    case SF_FORMAT_PCM_16:
      return WavSampleFormat::kPcm16;
    case SF_FORMAT_PCM_24:
      return WavSampleFormat::kPcm24;
    case SF_FORMAT_PCM_32:
      return WavSampleFormat::kPcm32;
    case SF_FORMAT_FLOAT:
      return WavSampleFormat::kFloat;
    case SF_FORMAT_DOUBLE:
      return WavSampleFormat::kDouble;
    default:
      return std::nullopt;
  }
}

bool IsWav(int format) {
  const int major = format & SF_FORMAT_TYPEMASK;
  return major == SF_FORMAT_WAV || major == SF_FORMAT_WAVEX;
}

}  // namespace

StatusOr<DecodedWav> SndfileWavReader::Read(const std::filesystem::path& path) {
  SF_INFO info = {};
  SndfileHandle file(sf_open(path.string().c_str(), SFM_READ, &info));
  if (!file) {
    return NotFoundError("cannot open " + path.string() + ": " +
                         sf_strerror(nullptr));
  }
  if (!IsWav(info.format)) {
    return InvalidArgumentError(path.string() + " is not a WAV file");
  }
  const std::optional<WavSampleFormat> format = SampleFormatOf(info.format);
  if (!format) {
    return InvalidArgumentError(path.string() +
                                ": unsupported WAV sample format");
  }
  if (info.channels < 1 || info.channels > kMaxChannels) {
    return InvalidArgumentError(path.string() + ": bad channel count " +
                                std::to_string(info.channels));
  }
  if (info.samplerate <= 0) {
    return InvalidArgumentError(path.string() + ": bad sample rate");
  }
  if (info.frames <= 0) {
    return DataLossError(path.string() + " holds no samples");
  }
  if (info.frames > kMaxFrames) {
    return OutOfRangeError(path.string() + " is too long");
  }
  const auto frames = static_cast<std::size_t>(info.frames);
  const auto channels = static_cast<std::size_t>(info.channels);
  std::vector<float> interleaved(frames * channels);
  // PCM is read normalised to -1..1 (libsndfile's default for floats).
  const sf_count_t got =
      sf_readf_float(file.get(), interleaved.data(), info.frames);
  if (got != info.frames) {
    return DataLossError(path.string() +
                         " is truncated: " + std::to_string(got) + " of " +
                         std::to_string(info.frames) + " frames read");
  }
  DecodedWav wav;
  wav.sample_rate = info.samplerate;
  wav.channels = info.channels;
  wav.format = *format;
  wav.samples.resize(frames);
  const float scale = 1.0F / static_cast<float>(channels);
  for (std::size_t i = 0; i < frames; ++i) {
    float sum = 0.0F;
    for (std::size_t c = 0; c < channels; ++c) {
      sum += interleaved[i * channels + c];
    }
    wav.samples[i] = sum * scale;
  }
  Logger(kLogModuleAudio)
      ->debug("read {}: {} Hz, {} channel(s), {} frames", path.string(),
              wav.sample_rate, wav.channels, frames);
  return wav;
}

Status SndfileWavWriter::Write(const std::filesystem::path& path,
                               std::span<const float> samples, int sample_rate,
                               WavOutputFormat format) {
  if (sample_rate <= 0) {
    return InvalidArgumentError("bad sample rate " +
                                std::to_string(sample_rate));
  }
  SF_INFO info = {};
  info.samplerate = sample_rate;
  info.channels = 1;
  info.format =
      SF_FORMAT_WAV |
      (format == WavOutputFormat::kPcm24 ? SF_FORMAT_PCM_24 : SF_FORMAT_PCM_16);
  SndfileHandle file(sf_open(path.string().c_str(), SFM_WRITE, &info));
  if (!file) {
    return UnavailableError("cannot write " + path.string() + ": " +
                            sf_strerror(nullptr));
  }
  // Clip rather than wrap samples outside -1..1.
  sf_command(file.get(), SFC_SET_CLIPPING, nullptr, SF_TRUE);
  const auto count = static_cast<sf_count_t>(samples.size());
  if (sf_writef_float(file.get(), samples.data(), count) != count) {
    return UnavailableError("cannot write " + path.string() + ": " +
                            sf_strerror(file.get()));
  }
  if (sf_close(file.release()) != 0) {
    return UnavailableError("cannot finish " + path.string());
  }
  return OkStatus();
}

}  // namespace speak5220::core::audio
