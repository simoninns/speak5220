# SPDX-FileCopyrightText: (c) 2026 Simon Inns
# SPDX-License-Identifier: GPL-3.0-or-later

# The entry point shared by the GUI test binaries: a QApplication on the
# offscreen platform, then Google Test.
add_library(speak5220_gui_test_main STATIC gui_test_main.cc)
target_link_libraries(
    speak5220_gui_test_main
    PUBLIC Qt6::Widgets GTest::gmock
    PRIVATE speak5220_compile_options
)
