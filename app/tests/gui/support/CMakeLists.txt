# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The entry point shared by the GUI test binaries (a QApplication on the
# offscreen platform, then Google Test) and the header-only GUI mocks in
# gui_mocks.h. The mocks' dependencies are public, so a GUI test binary
# links only this and speak5220_gui, and clang-tidy, which checks
# gui_mocks.h with this library's flags, finds its includes.
add_library(speak5220_gui_test_main STATIC gui_test_main.cpp)
target_link_libraries(
    speak5220_gui_test_main
    PUBLIC speak5220_gui speak5220_test_support Qt6::Widgets GTest::gmock
    PRIVATE speak5220_compile_options
)
