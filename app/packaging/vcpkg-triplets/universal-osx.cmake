# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The macOS DMG's libraries (cmake/Packaging.cmake): static, for arm64 and
# x86_64 in one, and for macOS 13.3, as the release workflow builds the
# application: the oldest macOS Qt 6.11 supports is 13.0, but libc++ has
# std::to_chars for floating point only from 13.3.
set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Darwin)
set(VCPKG_OSX_ARCHITECTURES "arm64;x86_64")
set(VCPKG_OSX_DEPLOYMENT_TARGET 13.3)
