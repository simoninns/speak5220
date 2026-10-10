# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# s5core: everything that produces a result, with no Qt GUI dependency
# (application plan §2.1).
add_library(s5core STATIC util/version.cc)

target_include_directories(s5core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(
    s5core
    PRIVATE SPEAK5220_VERSION="${PROJECT_VERSION}"
)
target_link_libraries(s5core PRIVATE speak5220_compile_options)
