# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# s5core: everything that produces a result, with no Qt GUI dependency
# (application plan §2.1). It depends on QtCore at most.
add_library(
    s5core
    STATIC
    audio/sndfile_wav_io.cc
    dsp/fft.cc
    dsp/resampler.cc
    emulator/chip_arithmetic.cc
    emulator/chip_driver.cc
    emulator/tms5220.cc
    emulator/tms5220_config.cc
    emulator/tms6100.cc
    lpc/frame.cc
    lpc/frame_codec.cc
    packaging/acorn_phrom.cc
    packaging/speak_external_stream.cc
    packaging/ti_indexed.cc
    synth/reference_synthesizer.cc
    synth/synth.cc
    util/logging.cc
    util/logging_options.cc
    util/status.cc
    util/version.cc
)

target_include_directories(s5core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(
    s5core
    PRIVATE SPEAK5220_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(
    s5core
    PUBLIC Qt6::Core spdlog::spdlog
    PRIVATE
        speak5220::sndfile
        speak5220::samplerate
        speak5220_pocketfft
        speak5220_compile_options
)
