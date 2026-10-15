# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

add_executable(speak5220-cli main.cpp cli_commands.cpp)
target_link_libraries(
    speak5220-cli
    PRIVATE s5core Qt6::Core speak5220_compile_options
)

# The macOS package has the command-line tool inside the application's
# bundle, beside the GUI, so the DMG holds the bundle alone
# (cmake/Packaging.cmake)
if(APPLE AND SPEAK5220_PACKAGE)
    install(
        TARGETS speak5220-cli
        RUNTIME DESTINATION speak5220.app/Contents/MacOS
    )
else()
    install(TARGETS speak5220-cli RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()
