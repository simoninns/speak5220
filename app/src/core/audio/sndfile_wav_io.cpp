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

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/audio/pcm24.h"
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

// Integer PCM is read as 32-bit integers, code << (32 - bits), so dividing
// by 2^31 gives code / 2^(bits - 1) exactly; libsndfile's own float
// conversion is not exact for 24-bit codes near full scale.
constexpr float kInt32Scale = 2147483648.0F;  // 2^31

bool IsIntegerPcm(WavSampleFormat format) {
  return format == WavSampleFormat::kPcm8 ||
         format == WavSampleFormat::kPcm16 || format == WavSampleFormat::kPcm24;
}

// A WAV file held in memory, for libsndfile's virtual I/O. Reading uses
// `input`; writing appends to (or overwrites within) `output`.
struct MemoryFile {
  std::span<const std::uint8_t> input;
  std::vector<std::uint8_t> output;
  sf_count_t position = 0;
  bool writing = false;

  sf_count_t Length() const {
    return static_cast<sf_count_t>(writing ? output.size() : input.size());
  }
};

MemoryFile& FileOf(void* user_data) {
  return *static_cast<MemoryFile*>(user_data);
}

sf_count_t MemoryLength(void* user_data) { return FileOf(user_data).Length(); }

sf_count_t MemorySeek(sf_count_t offset, int whence, void* user_data) {
  MemoryFile& file = FileOf(user_data);
  sf_count_t base = 0;
  if (whence == SEEK_CUR) {
    base = file.position;
  } else if (whence == SEEK_END) {
    base = file.Length();
  }
  const sf_count_t target = base + offset;
  if (target < 0) {
    return -1;
  }
  file.position = target;
  return file.position;
}

sf_count_t MemoryRead(void* destination, sf_count_t count, void* user_data) {
  MemoryFile& file = FileOf(user_data);
  const sf_count_t available =
      std::max<sf_count_t>(0, file.Length() - file.position);
  const sf_count_t got = std::clamp<sf_count_t>(count, 0, available);
  if (got > 0) {
    std::memcpy(
        destination,
        file.input.subspan(static_cast<std::size_t>(file.position)).data(),
        static_cast<std::size_t>(got));
  }
  file.position += got;
  return got;
}

sf_count_t MemoryWrite(const void* source, sf_count_t count, void* user_data) {
  MemoryFile& file = FileOf(user_data);
  if (count <= 0) {
    return 0;
  }
  const auto start = static_cast<std::size_t>(file.position);
  const auto length = static_cast<std::size_t>(count);
  if (file.output.size() < start + length) {
    file.output.resize(start + length);
  }
  std::memcpy(std::span(file.output).subspan(start).data(), source, length);
  file.position += count;
  return count;
}

sf_count_t MemoryTell(void* user_data) { return FileOf(user_data).position; }

SF_VIRTUAL_IO MemoryIo() {
  return {.get_filelen = MemoryLength,
          .seek = MemorySeek,
          .read = MemoryRead,
          .write = MemoryWrite,
          .tell = MemoryTell};
}

// Checks an opened file's format and size against the reader's limits.
// `what` names the file in messages.
StatusOr<WavSampleFormat> CheckInput(const SF_INFO& info,
                                     const std::string& what) {
  if (!IsWav(info.format)) {
    return InvalidArgumentError(what + " is not a WAV file");
  }
  const std::optional<WavSampleFormat> format = SampleFormatOf(info.format);
  if (!format) {
    return InvalidArgumentError(what + ": unsupported WAV sample format");
  }
  if (info.channels < 1 || info.channels > SndfileWavReader::kMaxChannels) {
    return InvalidArgumentError(what + ": bad channel count " +
                                std::to_string(info.channels));
  }
  if (info.samplerate <= 0) {
    return InvalidArgumentError(what + ": bad sample rate");
  }
  if (info.frames <= 0) {
    return DataLossError(what + " holds no samples");
  }
  if (info.frames > SndfileWavReader::kMaxFrames) {
    return OutOfRangeError(what + " is too long");
  }
  return *format;
}

// Reads every frame of an opened file and mixes the channels to mono by
// averaging. Integer PCM is read exactly (kInt32Scale); other formats as
// libsndfile's normalised floats.
StatusOr<DecodedWav> ReadAll(SNDFILE* file, const SF_INFO& info,
                             WavSampleFormat format, const std::string& what) {
  const auto frames = static_cast<std::size_t>(info.frames);
  const auto channels = static_cast<std::size_t>(info.channels);
  std::vector<float> interleaved(frames * channels);
  sf_count_t got = 0;
  if (IsIntegerPcm(format)) {
    std::vector<int> codes(frames * channels);
    got = sf_readf_int(file, codes.data(), info.frames);
    std::ranges::transform(codes, interleaved.begin(), [](int code) {
      return static_cast<float>(code) / kInt32Scale;
    });
  } else {
    got = sf_readf_float(file, interleaved.data(), info.frames);
  }
  if (got != info.frames) {
    return DataLossError(what + " is truncated: " + std::to_string(got) +
                         " of " + std::to_string(info.frames) + " frames read");
  }
  DecodedWav wav;
  wav.sample_rate = info.samplerate;
  wav.channels = info.channels;
  wav.format = format;
  wav.samples.resize(frames);
  const float scale = 1.0F / static_cast<float>(channels);
  for (std::size_t i = 0; i < frames; ++i) {
    float sum = 0.0F;
    for (std::size_t c = 0; c < channels; ++c) {
      sum += interleaved[i * channels + c];
    }
    wav.samples[i] = sum * scale;
  }
  return wav;
}

}  // namespace

StatusOr<DecodedWav> SndfileWavReader::Read(const std::filesystem::path& path) {
  SF_INFO info = {};
  SndfileHandle file(sf_open(path.string().c_str(), SFM_READ, &info));
  if (!file) {
    std::error_code error;
    if (!std::filesystem::is_regular_file(path, error)) {
      return NotFoundError(path.string() + " does not exist or is not a file");
    }
    // libsndfile's reason, such as "Format not recognised.".
    return InvalidArgumentError("cannot read " + path.string() +
                                " as a WAV file: " + sf_strerror(nullptr));
  }
  const StatusOr<WavSampleFormat> format = CheckInput(info, path.string());
  if (!format.IsOk()) {
    return format.GetStatus();
  }
  StatusOr<DecodedWav> wav = ReadAll(file.get(), info, *format, path.string());
  if (wav.IsOk()) {
    Logger(kLogModuleAudio)
        ->debug("read {}: {} Hz, {} channel(s), {} frames", path.string(),
                wav->sample_rate, wav->channels, wav->samples.size());
  }
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

StatusOr<DecodedWav> SndfileWavCodec::Decode(
    std::span<const std::uint8_t> bytes) {
  MemoryFile memory;
  memory.input = bytes;
  SF_VIRTUAL_IO io = MemoryIo();
  SF_INFO info = {};
  SndfileHandle file(sf_open_virtual(&io, SFM_READ, &info, &memory));
  if (!file) {
    return InvalidArgumentError(std::string("not a usable WAV file: ") +
                                sf_strerror(nullptr));
  }
  const std::string what = "the WAV data";
  const StatusOr<WavSampleFormat> format = CheckInput(info, what);
  if (!format.IsOk()) {
    return format.GetStatus();
  }
  return ReadAll(file.get(), info, *format, what);
}

StatusOr<std::vector<std::uint8_t>> SndfileWavCodec::Encode(
    std::span<const float> samples, int sample_rate, WavOutputFormat format) {
  if (sample_rate <= 0) {
    return InvalidArgumentError("bad sample rate " +
                                std::to_string(sample_rate));
  }
  MemoryFile memory;
  memory.writing = true;
  SF_VIRTUAL_IO io = MemoryIo();
  SF_INFO info = {};
  info.samplerate = sample_rate;
  info.channels = 1;
  info.format =
      SF_FORMAT_WAV |
      (format == WavOutputFormat::kPcm24 ? SF_FORMAT_PCM_24 : SF_FORMAT_PCM_16);
  SndfileHandle file(sf_open_virtual(&io, SFM_WRITE, &info, &memory));
  if (!file) {
    return InternalError(std::string("cannot start a WAV file: ") +
                         sf_strerror(nullptr));
  }
  // Written as 32-bit integers, code << 8, which libsndfile stores as the
  // 24-bit code itself; 16-bit output keeps the top 16 bits, rounded.
  constexpr int kPcm24Shift = 8;
  constexpr std::int32_t kPcm16Round = 0x80;
  std::vector<int> codes(samples.size());
  std::ranges::transform(samples, codes.begin(), [format](float sample) {
    const std::int32_t code = ToPcm24(sample);
    if (format == WavOutputFormat::kPcm16) {
      // Rounded to the nearest 16-bit code, clipped at full scale.
      const std::int32_t rounded = std::min(code + kPcm16Round, kPcm24Max);
      return static_cast<int>(static_cast<std::uint32_t>(rounded & ~0xFF)
                              << kPcm24Shift);
    }
    return static_cast<int>(static_cast<std::uint32_t>(code) << kPcm24Shift);
  });
  const auto count = static_cast<sf_count_t>(codes.size());
  if (sf_writef_int(file.get(), codes.data(), count) != count) {
    return InternalError(std::string("cannot write WAV data: ") +
                         sf_strerror(file.get()));
  }
  if (sf_close(file.release()) != 0) {
    return InternalError("cannot finish the WAV data");
  }
  return std::move(memory.output);
}

}  // namespace speak5220::core::audio
