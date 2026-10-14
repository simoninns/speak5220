# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# s5core: everything that produces a result, with no Qt GUI dependency
# (application plan §2.1). It depends on QtCore at most.
add_library(
    s5core
    STATIC
    audio/sndfile_wav_io.cpp
    bbc/basic_tokeniser.cpp
    bbc/bbc_disc.cpp
    bbc/dfs_disc.cpp
    dsp/array_math.cpp
    dsp/de_esser.cpp
    dsp/envelope.cpp
    dsp/fft.cpp
    dsp/iir_filter.cpp
    dsp/lpc_analysis.cpp
    dsp/noise_gate.cpp
    dsp/output_filter.cpp
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
    lpc/frame_statistics.cpp
    optimiser/optimiser.cpp
    preprocess/preprocess.cpp
    packaging/acorn_phrom.cpp
    packaging/c_header.cpp
    packaging/speak_external_stream.cpp
    packaging/ti_indexed.cpp
    project/c_identifier.cpp
    project/directory_project_store.cpp
    project/encoding_cache.cpp
    project/phrase.cpp
    project/phrase_importer.cpp
    project/phrase_name.cpp
    project/phrom_allocator.cpp
    project/project.cpp
    project/project_export.cpp
    project/project_migration.cpp
    project/project_serializer.cpp
    project/project_store.cpp
    project/zip_project_store.cpp
    quality/anchor_vocoder.cpp
    quality/cached_quality.cpp
    quality/encoded_audio.cpp
    quality/envelope_distortion.cpp
    quality/estoi.cpp
    quality/frame_track_comparison.cpp
    quality/loudness.cpp
    quality/lpc_envelope.cpp
    quality/numpy_normal.cpp
    quality/output_alignment.cpp
    quality/pitch_errors.cpp
    quality/pitch_estimator.cpp
    quality/quality_assessor.cpp
    quality/quality_measures.cpp
    quality/quality_score.cpp
    quality/signal_preparation.cpp
    quality/voice_activity.cpp
    synth/output_pin.cpp
    synth/reference_synthesizer.cpp
    synth/synth.cpp
    util/clock.cpp
    util/logging.cpp
    util/file_digest.cpp
    util/logging_options.cpp
    util/number_text.cpp
    util/sha256.cpp
    util/status.cpp
    util/task_runner.cpp
    util/version.cpp
)

target_include_directories(s5core PUBLIC "${PROJECT_SOURCE_DIR}/src")

# The version is the git commit hash, with "-dirty" for uncommitted changes
# (cmake/GitVersion.cmake). It is found at every build, not at configure
# time, so it follows commits made without reconfiguring.
set(SPEAK5220_GIT_VERSION
    ""
    CACHE STRING
    "Version to report instead of asking git (the Nix package sets it)"
)
set(_version_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_target(
    s5version
    COMMAND
        "${CMAKE_COMMAND}" "-DSOURCE_DIR=${PROJECT_SOURCE_DIR}"
        "-DOUTPUT=${_version_dir}/core/util/version_string.h"
        "-DGIT_VERSION=${SPEAK5220_GIT_VERSION}" -P
        "${PROJECT_SOURCE_DIR}/cmake/GitVersion.cmake"
    BYPRODUCTS "${_version_dir}/core/util/version_string.h"
    COMMENT "Finding the version from git"
    VERBATIM
)
add_dependencies(s5core s5version)

# The BBC Micro disc's player program, kept as BASIC text and built into
# s5core as a string (core/bbc/bbc_disc.h).
set(_player_header "${_version_dir}/core/bbc/player_source.h")
add_custom_command(
    OUTPUT "${_player_header}"
    COMMAND
        "${CMAKE_COMMAND}" "-DINPUT=${CMAKE_CURRENT_SOURCE_DIR}/bbc/speak.bas"
        "-DOUTPUT=${_player_header}"
        "-DGUARD=SPEAK5220_CORE_BBC_PLAYER_SOURCE_H_"
        "-DNAMESPACE=speak5220::core::bbc" "-DNAME=kPlayerSource" -P
        "${PROJECT_SOURCE_DIR}/cmake/EmbedText.cmake"
    DEPENDS
        "${CMAKE_CURRENT_SOURCE_DIR}/bbc/speak.bas"
        "${PROJECT_SOURCE_DIR}/cmake/EmbedText.cmake"
    COMMENT "Embedding the BBC Micro disc's player program"
    VERBATIM
)
target_sources(s5core PRIVATE "${_player_header}")
target_include_directories(s5core PRIVATE "${_version_dir}")
target_link_libraries(
    s5core
    PUBLIC Qt6::Core spdlog::spdlog yaml-cpp::yaml-cpp
    PRIVATE
        libzip::zip
        speak5220::sndfile
        speak5220::samplerate
        speak5220_pocketfft
        speak5220::fvad
        SPTK::sptk
        speak5220_compile_options
)
