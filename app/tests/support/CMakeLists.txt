# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# Shared helpers for every test binary: frame builders, golden-file readers,
# the reference emulator's run modes and the gMock mocks of the core
# interfaces. Includes are written from app/tests/ ("support/...").
add_library(
    speak5220_test_support
    STATIC
    emulator_run.cc
    frame_builder.cc
    golden_data.cc
)
target_include_directories(
    speak5220_test_support
    PUBLIC "${PROJECT_SOURCE_DIR}/tests"
)
target_link_libraries(
    speak5220_test_support
    PUBLIC s5core GTest::gmock
    PRIVATE speak5220_compile_options
)
