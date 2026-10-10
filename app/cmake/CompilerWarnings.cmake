# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# Compiler settings shared by every target in app/ (application plan §1.3).
# Targets link speak5220_compile_options instead of setting flags themselves.

add_library(speak5220_compile_options INTERFACE)

if(MSVC)
    # /utf-8: the sources are UTF-8 (they cite datasheet sections with "§").
    # /external: third-party headers (Qt, gtest, spdlog) are not ours to
    # warn about. _CRT_SECURE_NO_WARNINGS: std::getenv and the other
    # standard functions MSVC marks deprecated are standard C++.
    target_compile_options(
        speak5220_compile_options
        INTERFACE
            /W4
            /WX
            /utf-8
            /permissive-
            /external:anglebrackets
            /external:W0
    )
    target_compile_definitions(
        speak5220_compile_options
        INTERFACE _CRT_SECURE_NO_WARNINGS
    )
else()
    # -ffp-contract=off: no fused multiply-add contraction, which Clang
    # applies by default (and GCC where the target has FMA), so the
    # encoder's floating point rounds as NumPy's does on every platform
    # (plan §3.2). -ffast-math is never used.
    target_compile_options(
        speak5220_compile_options
        INTERFACE
            -Wall
            -Wextra
            -Wpedantic
            -Wconversion
            -Wshadow
            -Werror
            -ffp-contract=off
    )
endif()

# Range-checks operator[] on standard containers in Debug and sanitizer
# builds (libstdc++), in place of clang-tidy's static index rule.
if(NOT MSVC)
    target_compile_definitions(
        speak5220_compile_options
        INTERFACE
            $<$<OR:$<CONFIG:Debug>,$<BOOL:${SPEAK5220_SANITIZE}>>:_GLIBCXX_ASSERTIONS>
    )
endif()

if(SPEAK5220_SANITIZE)
    if(MSVC)
        message(
            FATAL_ERROR
            "SPEAK5220_SANITIZE is supported with GCC and Clang only"
        )
    endif()
    set(_sanitize_flags
        -fsanitize=address,undefined
        -fno-sanitize-recover=undefined
        -fno-omit-frame-pointer
    )
    target_compile_options(
        speak5220_compile_options
        INTERFACE ${_sanitize_flags}
    )
    target_link_options(speak5220_compile_options INTERFACE ${_sanitize_flags})
endif()
