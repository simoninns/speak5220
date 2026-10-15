# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# Shared helpers for every test binary: frame builders, golden-file readers,
# tms5220emu's run modes and the gMock mocks of the core
# interfaces. Includes are written from app/tests/ ("support/...").
add_library(
    speak5220_test_support
    STATIC
    emulator_run.cpp
    encoder_golden.cpp
    frame_builder.cpp
    golden_data.cpp
    measures_golden.cpp
    project_builder.cpp
    quality_mocks.cpp
    synthetic_speech.cpp
    tms5220emu_command.cpp
)
target_include_directories(
    speak5220_test_support
    PUBLIC "${PROJECT_SOURCE_DIR}/tests"
)
target_link_libraries(
    speak5220_test_support
    PUBLIC s5core Qt6::Core GTest::gmock
    PRIVATE speak5220_compile_options
)
# The encoder's reference readers default to the committed data.
target_compile_definitions(
    speak5220_test_support
    PRIVATE SPEAK5220_GOLDEN_DIR="${PROJECT_SOURCE_DIR}/golden"
)

# The retired C emulator's command line on the application's emulator, for
# the reference-data and research scripts (docs/technical/emulator/
# command-line.md). Built with the tests, never installed.
add_executable(tms5220emu tms5220emu_main.cpp)
target_link_libraries(
    tms5220emu
    PRIVATE speak5220_test_support speak5220_compile_options
)
add_dependencies(speak5220_tests tms5220emu)
