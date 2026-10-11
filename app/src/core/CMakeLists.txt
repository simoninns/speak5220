# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# s5core: everything that produces a result, with no Qt GUI dependency
# (application plan §2.1). It depends on QtCore at most.
add_library(
    s5core
    STATIC
    audio/sndfile_wav_io.cpp
    dsp/array_math.cpp
    dsp/fft.cpp
    dsp/iir_filter.cpp
    dsp/lpc_analysis.cpp
    dsp/resampler.cpp
    emulator/chip_arithmetic.cpp
    emulator/chip_driver.cpp
    emulator/tms5220.cpp
    emulator/tms5220_config.cpp
    emulator/tms6100.cpp
    encoder/encoder.cpp
    encoder/encoder_filters.cpp
    encoder/encoder_options.cpp
    encoder/frame_analyser.cpp
    encoder/frame_endings.cpp
    encoder/level_setter.cpp
    encoder/pitch_tracker.cpp
    encoder/quantiser.cpp
    encoder/repeat_search.cpp
    encoder/voicing_classifier.cpp
    lpc/frame.cpp
    lpc/frame_codec.cpp
    packaging/acorn_phrom.cpp
    packaging/speak_external_stream.cpp
    packaging/ti_indexed.cpp
    quality/anchor_vocoder.cpp
    quality/cheap_measures.cpp
    quality/numpy_normal.cpp
    quality/output_alignment.cpp
    quality/pesq.cpp
    quality/pesq_alignment.cpp
    quality/pesq_fft.cpp
    quality/pesq_metric.cpp
    quality/pesq_model.cpp
    quality/quality_scale.cpp
    synth/reference_synthesizer.cpp
    synth/synth.cpp
    util/logging.cpp
    util/logging_options.cpp
    util/status.cpp
    util/version.cpp
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
