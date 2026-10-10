# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# Shared helpers for every test binary: fixture paths, golden-file readers and
# the gMock mocks of the core interfaces, as they are added. Includes are
# written from app/tests/ ("support/...").
add_library(speak5220_test_support INTERFACE)
target_include_directories(
    speak5220_test_support
    INTERFACE "${PROJECT_SOURCE_DIR}/tests"
)
target_link_libraries(speak5220_test_support INTERFACE GTest::gmock)
