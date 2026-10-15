# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

cmake_minimum_required(VERSION 3.24)

# Out-of-source builds only. A build directory
# anywhere inside app/ is refused, not only app/ itself.
cmake_path(
    IS_PREFIX
    CMAKE_SOURCE_DIR
    "${CMAKE_BINARY_DIR}"
    NORMALIZE
    _in_source
)
if(_in_source)
    message(
        FATAL_ERROR
        "In-source builds are not allowed: the build directory "
        "(${CMAKE_BINARY_DIR}) is inside the source tree "
        "(${CMAKE_SOURCE_DIR}).\n"
        "Remove the CMakeCache.txt and CMakeFiles/ just created there, then "
        "configure from the repository root:\n"
        "  cmake -S app -B build/app -G Ninja"
    )
endif()

project(
    speak5220
    DESCRIPTION "TMS5220 LPC speech data and PHROM editor"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(SPEAK5220_BUILD_TESTS "Build the Google Test suites" ON)
option(SPEAK5220_SANITIZE "Build with AddressSanitizer and UBSan" OFF)
option(
    SPEAK5220_PACKAGE
    "Bundle Qt into the installed application and build the Windows MSI or macOS DMG with CPack"
    OFF
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(GNUInstallDirs)
include(CompilerWarnings)
include(QualityTargets)
include(Dependencies)

add_subdirectory(src)

if(SPEAK5220_PACKAGE)
    include(Packaging)
endif()

if(SPEAK5220_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
