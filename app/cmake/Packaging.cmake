# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The Windows MSI and the macOS DMG, with SPEAK5220_PACKAGE=ON. Installing
# then also bundles Qt beside the programs (windeployqt or macdeployqt), and
# cpack builds the installer from what is installed. The other libraries
# are linked statically (the release workflow's vcpkg triplets), so Qt is
# all there is to bundle. Linux is packaged as a Flatpak instead
# (app/packaging/flatpak/).

if(NOT (WIN32 OR APPLE))
    message(
        FATAL_ERROR
        "SPEAK5220_PACKAGE builds the Windows MSI and the macOS DMG; Linux "
        "is packaged as a Flatpak (app/packaging/flatpak/)"
    )
endif()

# The installer's version, three numbers as an MSI needs them; the release
# workflow sets it from the tag. The programs still report their commit
# (cmake/GitVersion.cmake).
set(SPEAK5220_PACKAGE_VERSION
    "0.0.0"
    CACHE STRING
    "The installer's version, MAJOR.MINOR.PATCH"
)
if(NOT SPEAK5220_PACKAGE_VERSION MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+$")
    message(
        FATAL_ERROR
        "SPEAK5220_PACKAGE_VERSION (${SPEAK5220_PACKAGE_VERSION}) must be "
        "three numbers, such as 1.2.3"
    )
endif()

# Qt's libraries and plugins, and a qt.conf, beside the programs. On macOS
# both programs are in the bundle (src/cli/CMakeLists.txt), which is then
# signed ad hoc: macdeployqt changes the binaries, breaking the signatures
# the linker gave them, and Apple silicon runs no unsigned code. It is not
# notarised: the project uses no paid services, and notarising needs a paid
# Apple developer account. QT_DEPLOY_PREFIX, which includes DESTDIR, is
# escaped twice: once for set() and once for qt_generate_deploy_script().
if(APPLE)
    set(_bundle "$<TARGET_FILE_NAME:speak5220>.app")
    set(_executable "${_bundle}")
    set(_cli "${_bundle}/Contents/MacOS/$<TARGET_FILE_NAME:speak5220-cli>")
    set(_deploy_options "")
    set(_after_deploy
        "execute_process(COMMAND codesign --force --deep --sign - \"\\\${QT_DEPLOY_PREFIX}/${_bundle}\" COMMAND_ERROR_IS_FATAL ANY)"
    )
else()
    set(_executable "${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:speak5220>")
    set(_cli "${CMAKE_INSTALL_BINDIR}/$<TARGET_FILE_NAME:speak5220-cli>")
    # The compiler's runtime comes from InstallRequiredSystemLibraries below
    set(_deploy_options "NO_COMPILER_RUNTIME")
    set(_after_deploy "")
endif()
qt_generate_deploy_script(
    TARGET speak5220
    OUTPUT_SCRIPT _deploy_script
    CONTENT
        "
qt_deploy_runtime_dependencies(
    EXECUTABLE \"${_executable}\"
    ADDITIONAL_EXECUTABLES \"${_cli}\"
    GENERATE_QT_CONF
    NO_TRANSLATIONS
    ${_deploy_options}
)
${_after_deploy}
"
)
install(SCRIPT "${_deploy_script}")

# The MSVC runtime's DLLs, beside the programs, so the MSI needs no
# separate redistributable
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION ${CMAKE_INSTALL_BINDIR})
    include(InstallRequiredSystemLibraries)
endif()

# The licence the MSI shows; WiX takes a .txt or .rtf file
set(_licence "${CMAKE_BINARY_DIR}/LICENSE.txt")
configure_file("${PROJECT_SOURCE_DIR}/../LICENSE" "${_licence}" COPYONLY)

set(CPACK_PACKAGE_NAME Speak5220)
set(CPACK_PACKAGE_VENDOR "Simon Inns")
set(CPACK_PACKAGE_VERSION "${SPEAK5220_PACKAGE_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/simoninns/speak5220")
set(CPACK_PACKAGE_INSTALL_DIRECTORY Speak5220)
set(CPACK_RESOURCE_FILE_LICENSE "${_licence}")
# A .sha256 file beside each installer, for the release
set(CPACK_PACKAGE_CHECKSUM SHA256)

if(WIN32)
    set(CPACK_GENERATOR WIX)
    # The WiX .NET tool (wix.exe) and its UI extension, version 4
    set(CPACK_WIX_VERSION 4)
    # The same for every version, so a newer MSI replaces an older one
    set(CPACK_WIX_UPGRADE_GUID FF25AE27-AB46-407A-961B-D089D42C8C6D)
    set(CPACK_WIX_PRODUCT_ICON "${PROJECT_SOURCE_DIR}/resources/speak5220.ico")
    set(CPACK_WIX_PROGRAM_MENU_FOLDER Speak5220)
    # A Start menu shortcut to the GUI
    set(CPACK_PACKAGE_EXECUTABLES speak5220 Speak5220)
    set(CPACK_PACKAGE_FILE_NAME
        "Speak5220-${SPEAK5220_PACKAGE_VERSION}-windows-x64"
    )
else()
    set(CPACK_GENERATOR DragNDrop)
    set(CPACK_DMG_VOLUME_NAME Speak5220)
    # "universal" for an arm64 and x86_64 build
    if(
        CMAKE_OSX_ARCHITECTURES MATCHES "arm64"
        AND CMAKE_OSX_ARCHITECTURES MATCHES "x86_64"
    )
        set(_architecture universal)
    elseif(CMAKE_OSX_ARCHITECTURES)
        set(_architecture "${CMAKE_OSX_ARCHITECTURES}")
    else()
        set(_architecture "${CMAKE_HOST_SYSTEM_PROCESSOR}")
    endif()
    set(CPACK_PACKAGE_FILE_NAME
        "Speak5220-${SPEAK5220_PACKAGE_VERSION}-macos-${_architecture}"
    )
endif()

include(CPack)
