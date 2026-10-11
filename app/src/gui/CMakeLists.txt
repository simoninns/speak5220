# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The GUI's widgets are a library, so the GUI tests can link them; the
# speak5220 executable adds only main().
add_library(speak5220_gui STATIC main_window/main_window.cpp)

set_target_properties(speak5220_gui PROPERTIES AUTOMOC ON)
target_include_directories(speak5220_gui PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_link_libraries(
    speak5220_gui
    PUBLIC s5core Qt6::Widgets
    PRIVATE speak5220_compile_options
)

add_executable(speak5220 WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(speak5220 PRIVATE speak5220_gui speak5220_compile_options)

install(
    TARGETS speak5220
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION .
)
