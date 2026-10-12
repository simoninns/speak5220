# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# SPTK 4.4, the version nixpkgs packages for flake.nix: the library only,
# without its command-line tools.
vcpkg_check_linkage(ONLY_STATIC_LIBRARY)

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO sp-nitech/SPTK
    REF "v${VERSION}"
    SHA512 21f4b09a28a1143aa0d345e86f08e82dd696bf004778346e75a839947fb8302195e31a58e9583d762f2b7876992b64588f3ca80365d58f2407b234053cde5293
    HEAD_REF master
)

vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSPTK_BUILD_COMMANDS=OFF
)
vcpkg_cmake_install()
vcpkg_cmake_config_fixup(PACKAGE_NAME SPTK CONFIG_PATH lib/cmake/SPTK)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
