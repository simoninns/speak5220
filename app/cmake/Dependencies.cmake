# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# Third-party libraries used by s5core (application plan §2.4). Each one is
# exposed as an imported target, so its headers are system headers and the
# project's warning flags do not apply to them.

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Widgets)
find_package(spdlog 1.12 CONFIG REQUIRED)
# The project file (plan §4): YAML metadata in a ZIP container
find_package(yaml-cpp 0.7 CONFIG REQUIRED)
find_package(libzip 1.7 CONFIG REQUIRED)

# libsndfile and libsamplerate, found by header and library. Their pkg-config
# files list private dependencies (FLAC, Vorbis, ...) that a shared build
# does not need at link time, so pkg-config is not used.
function(speak5220_import_library target header library)
    string(TOUPPER "${target}" upper)
    find_path(SPEAK5220_${upper}_INCLUDE_DIR ${header} REQUIRED)
    find_library(SPEAK5220_${upper}_LIBRARY ${library} REQUIRED)
    add_library(speak5220::${target} UNKNOWN IMPORTED)
    set_target_properties(
        speak5220::${target}
        PROPERTIES
            IMPORTED_LOCATION "${SPEAK5220_${upper}_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${SPEAK5220_${upper}_INCLUDE_DIR}"
    )
endfunction()

speak5220_import_library(sndfile sndfile.h sndfile)
speak5220_import_library(samplerate samplerate.h samplerate)

# pocketfft is header-only and ships no CMake or pkg-config file
find_path(SPEAK5220_POCKETFFT_INCLUDE_DIR pocketfft_hdronly.h REQUIRED)
add_library(speak5220_pocketfft INTERFACE)
target_include_directories(
    speak5220_pocketfft
    SYSTEM
    INTERFACE "${SPEAK5220_POCKETFFT_INCLUDE_DIR}"
)
# One thread per transform: results do not depend on a thread pool
# (TESTING.md, "Determinism requirements")
target_compile_definitions(
    speak5220_pocketfft
    INTERFACE POCKETFFT_NO_MULTITHREADING
)
